import time
from motor.motor_asyncio import AsyncIOMotorClient
from pymongo import UpdateOne

class Database:
    def __init__(self, config):
        self.client = AsyncIOMotorClient(config['db']['host'], config['db']['port'])
        self.db = self.client[config['db']['name']]
        self.collection = self.db[config['db']['collection']]
        # Запоминаем лимит из конфига
        self.max_limit = config['logic']['max_documents']
        
    async def init_indices(self):
        """Создаем индексы для уникальности и скорости выборки"""
        await self.collection.create_index("url", unique=True)
        await self.collection.create_index([("status", 1), ("next_check", 1)])
        await self.collection.create_index("source")

    async def add_links_to_queue(self, links, source_name):
        """Массовое добавление ссылок с проверкой общего лимита"""
        if not links:
            return

        current_total = await self.collection.estimated_document_count()
        
        if current_total >= self.max_limit:
            return

        free_space = self.max_limit - current_total
        if len(links) > free_space:
            links = links[:free_space]

        operations = []
        for link in links:
            op = UpdateOne(
                {"url": link},
                {
                    "$setOnInsert": {
                        "url": link,
                        "source": source_name,
                        "status": "pending",
                        "next_check": 0
                    }
                },
                upsert=True
            )
            operations.append(op)
        
        try:
            # ordered=False быстрее, так как не останавливается при ошибках дублей
            await self.collection.bulk_write(operations, ordered=False)
        except Exception:
            pass 

    async def get_batch_to_crawl(self, batch_size=50):
        current_time = time.time()
        final_batch = []
        
        sources = await self.collection.distinct("source")
        
        if not sources:
            return []

        limit_per_source = batch_size // len(sources) + 1
        
        for source in sources:
            cursor = self.collection.find({
                "source": source,
                "status": {"$in": ["pending", "done"]},
                "next_check": {"$lte": current_time}
            }).limit(limit_per_source)
            
            docs = await cursor.to_list(length=limit_per_source)
            final_batch.extend(docs)
        
        return final_batch
    
    async def save_document(self, url, html, content_hash, recrawl_interval, update_only=False):
        """Сохраняет документ"""
        current_time = time.time()
        
        update_data = {
            "downloaded_at": current_time,
            "status": "done",
            "content_hash": content_hash,
            "next_check": current_time + recrawl_interval,
            "last_error": None
        }
        
        if not update_only:
            update_data["html"] = html

        await self.collection.update_one(
            {"url": url},
            {"$set": update_data}
        )

    async def mark_error(self, url, error_msg=None):
        """Помечает ошибку"""
        update_data = {
            "status": "error", 
            "next_check": time.time() + 3600
        }
        
        if error_msg:
            update_data["last_error"] = error_msg

        await self.collection.update_one(
            {"url": url},
            {"$set": update_data}
        )

    async def count_documents(self):
        return await self.collection.count_documents({"status": "done"})
    
    async def count_pending(self):
        return await self.collection.count_documents({"status": "pending"})