import asyncio
import aiohttp
import hashlib
import gzip
import time
import ssl
from urllib.parse import urlparse, urljoin, urlunparse
from bs4 import BeautifulSoup
from .db import Database

class Crawler:
    def __init__(self, config, db: Database):
        self.config = config
        self.db = db
        self.sem = asyncio.Semaphore(config['logic']['concurrency'])
        self.session = None
        
        self.max_documents = config['logic']['max_documents']
        self.current_count = 0
        self.stop_signal = False
        
        self.sources_map = {s['root_url']: s for s in config['sources']}
        self.allowed_domains = set()
        for s in config['sources']:
            self.allowed_domains.update(s['allowed_domains'])

    def normalize_url(self, url):
        parsed = urlparse(url)
        return urlunparse((parsed.scheme, parsed.netloc.lower(), parsed.path, '', '', ''))

    def is_allowed(self, url):
        """Проверка домена и поддоменов"""
        domain = urlparse(url).netloc.lower()
        if domain.startswith('www.'):
            domain = domain[4:]
            
        for allowed in self.allowed_domains:
            clean_allowed = allowed[4:] if allowed.startswith('www.') else allowed
            if domain == clean_allowed or domain.endswith('.' + clean_allowed):
                return True
        return False

    async def init_session(self):
        timeout = aiohttp.ClientTimeout(total=120, connect=20)
        
        headers = {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7",
            "Accept-Language": "ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7",
            "Accept-Encoding": "gzip, deflate, br",
            "Connection": "keep-alive",
            "Upgrade-Insecure-Requests": "1",
            "Sec-Ch-Ua": '"Not_A Brand";v="8", "Chromium";v="120", "Google Chrome";v="120"',
            "Sec-Ch-Ua-Mobile": "?0",
            "Sec-Ch-Ua-Platform": '"Windows"',
            "Sec-Fetch-Dest": "document",
            "Sec-Fetch-Mode": "navigate",
            "Sec-Fetch-Site": "none",
            "Sec-Fetch-User": "?1",
            "Cache-Control": "max-age=0"
        }
        
        ssl_context = ssl.create_default_context()
        ssl_context.check_hostname = False
        ssl_context.verify_mode = ssl.CERT_NONE
        
        connector = aiohttp.TCPConnector(ssl=ssl_context, limit=None, ttl_dns_cache=300)
        
        self.session = aiohttp.ClientSession(
            connector=connector,
            headers=headers, 
            timeout=timeout,
            cookie_jar=aiohttp.CookieJar(unsafe=True)
        )

    async def close(self):
        if self.session:
            await self.session.close()

    def get_source_info(self, url):
        domain = urlparse(url).netloc.lower()
        if domain.startswith('www.'):
            domain = domain[4:]
            
        for source in self.config['sources']:
            for allowed in source['allowed_domains']:
                clean_allowed = allowed[4:] if allowed.startswith('www.') else allowed
                if domain == clean_allowed or domain.endswith('.' + clean_allowed):
                    return source['name']
        return "Unknown"

    def extract_links_from_html(self, html, base_url):
        try:
            soup = BeautifulSoup(html, 'lxml')
            links = set()
            
            ignored_extensions = (
                '.xml', '.xml.gz', '.gz', '.pdf', '.jpg', '.jpeg', '.png', 
                '.gif', '.css', '.js', '.ico', '.svg', '.zip', '.rar'
            )

            for a_tag in soup.find_all('a', href=True):
                href = a_tag['href']
                full_url = urljoin(base_url, href)
                parsed = urlparse(full_url)
                
                if parsed.scheme in ['http', 'https'] and self.is_allowed(full_url):
                    path_lower = parsed.path.lower()
                    if path_lower.endswith(ignored_extensions):
                        continue
                    normalized = self.normalize_url(full_url)
                    links.add(normalized)
            return list(links)
        except Exception:
            return []

    # --- SITEMAP & RSS LOGIC ---

    def should_process_sitemap(self, url, source_root):
        url_lower = url.lower()
        if "rss" in url_lower: return True # Всегда разрешаем RSS
        if url_lower.endswith('/sitemap.xml') or url_lower.endswith('/sitemap_index.xml'):
            return True
        if "sitemap_index" in url_lower:
            return True
        
        root_path = urlparse(source_root).path
        if not root_path or root_path == "/":
            return True
        if root_path not in urlparse(url).path:
            return False
        return True

    async def fetch_robots_txt(self, root_url):
        robots_url = urljoin(root_url, "/robots.txt")
        try:
            proxy = self.config['logic'].get('proxy')
            async with self.session.get(robots_url, proxy=proxy, timeout=15) as response:
                if response.status == 200:
                    text = await response.text()
                    sitemaps = []
                    for line in text.splitlines():
                        if line.lower().startswith("sitemap:"):
                            parts = line.split(":", 1)
                            if len(parts) > 1:
                                sitemaps.append(parts[1].strip())
                    return sitemaps
        except Exception:
            pass
        return []

    async def process_sitemap(self, url, source_name, source_root):
        if not self.should_process_sitemap(url, source_root):
            return

        print(f"[{source_name}] Processing Sitemap/RSS: {url}")
        
        content = None
        proxy = self.config['logic'].get('proxy')

        for attempt in range(3):
            try:
                async with self.session.get(url, proxy=proxy) as response:
                    if response.status != 200:
                        print(f"[{source_name}] Error {response.status} for {url}")
                        return 
                    content = await response.read()
                    break 
            except Exception as e:
                if attempt == 2:
                    print(f"[{source_name}] Failed to download {url}: {e}")
                    return
                await asyncio.sleep(1)
        
        if not content:
            return

        try:
            if url.endswith('.gz') or content[:2] == b'\x1f\x8b':
                try:
                    content = gzip.decompress(content)
                except Exception:
                    pass 

            soup = BeautifulSoup(content, 'xml')
            
            # 1. Индекс карт
            sitemaps = soup.find_all('sitemap')
            if sitemaps:
                tasks = []
                for sm in sitemaps:
                    loc = sm.find('loc')
                    if loc:
                        tasks.append(self.process_sitemap(loc.text.strip(), source_name, source_root))
                if tasks:
                    await asyncio.gather(*tasks)
            
            # 2. Обычный Sitemap + RSS (item/link)
            else:
                extracted_links = []
                # Sitemap standard
                urls = soup.find_all('url')
                for u in urls:
                    loc = u.find('loc')
                    if loc: extracted_links.append(loc.text.strip())
                
                # RSS standard (ДЛЯ РБК!)
                items = soup.find_all('item')
                for item in items:
                    link = item.find('link')
                    if link: extracted_links.append(link.text.strip())

                valid_links = []
                for link in extracted_links:
                    if self.get_source_info(link) == source_name:
                        if source_name == "DW" and "/ru/" not in link:
                            continue
                        valid_links.append(self.normalize_url(link))
                
                if valid_links:
                    print(f"[{source_name}] Found {len(valid_links)} links in {url}")
                    await self.db.add_links_to_queue(valid_links, source_name)

        except Exception as e:
            print(f"[{source_name}] Sitemap parse error {url}: {e}")

    async def process_source_bootstrap(self, source):
        print(f"[{source['name']}] Looking for sitemaps/RSS...")
        
        sitemaps = await self.fetch_robots_txt(source['root_url'])
        
        if source['name'] == 'RBC':
            print(f"[{source['name']}] Injecting RSS feeds...")
            rbc_feeds = [
                "http://static.feed.rbc.ru/rbc/logical/footer/news.rss",      # Официальная лента
                "https://rssexport.rbc.ru/rbcnews/news/30/full.rss",          # Альтернативный экспорт
                "http://static.feed.rbc.ru/rbc/internal/news.rbc.ru/news.rss" # Внутренняя лента
            ]
            sitemaps.extend(rbc_feeds)

        if not sitemaps:
            potential = [
                urljoin(source['root_url'], "/sitemap.xml"),
                urljoin(source['root_url'], "/sitemap_index.xml")
            ]
            sitemaps.extend(potential)

        # Удаляем дубликаты
        sitemaps = list(set(sitemaps))
        
        if sitemaps:

            tasks = [self.process_sitemap(sm, source['name'], source['root_url']) for sm in sitemaps]
            await asyncio.gather(*tasks)

        total = await self.db.collection.count_documents({"source": source['name']})
        if total <= 1: 
             print(f"[{source['name']}] Still empty. Adding root URL again.")
             await self.db.add_links_to_queue([source['root_url']], source['name'])

    # --- CRAWLING LOGIC ---

    async def process_page(self, doc):
        if self.stop_signal:
            return

        url = doc['url']
        current_hash = doc.get('content_hash')
        was_done_before = doc.get('status') == 'done'
        
        if self.current_count >= self.max_documents and not was_done_before:
            return

        async with self.sem:
            try:
                await asyncio.sleep(self.config['logic']['delay'])
                proxy = self.config['logic'].get('proxy')
                
                async with self.session.get(url, proxy=proxy) as response:
                    if response.status == 200:
                        content = await response.read()
                        try:
                            text = content.decode('utf-8')
                        except UnicodeDecodeError:
                            text = content.decode('cp1251', errors='ignore')

                        new_hash = hashlib.md5(content).hexdigest()
                        
                        if current_hash == new_hash and was_done_before:
                             await self.db.save_document(url, None, new_hash, 
                                                         self.config['logic']['recrawl_interval'], update_only=True)
                        else:
                            await self.db.save_document(url, text, new_hash, 
                                                        self.config['logic']['recrawl_interval'], update_only=False)
                            
                            if not was_done_before:
                                self.current_count += 1
                                if self.current_count % 50 == 0:
                                    print(f"Progress: {self.current_count}/{self.max_documents}")

                        # Extract links
                        if self.current_count < self.max_documents:
                            new_links = self.extract_links_from_html(text, url)
                            if new_links:
                                source_name = self.get_source_info(url)
                                await self.db.add_links_to_queue(new_links, source_name)
                    else:
                        error_msg = f"HTTP {response.status}"
                        await self.db.mark_error(url, error_msg)

            except Exception as e:
                error_msg = str(e)
                await self.db.mark_error(url, error_msg)

    async def run(self):
        await self.db.init_indices()
        await self.init_session()
        
        self.current_count = await self.db.count_documents()
        print(f"Database contains: {self.current_count} documents.")
        
        print("Checking sources status...")
        bootstrap_tasks = []
        
        for source in self.config['sources']:
            count = await self.db.collection.count_documents({"source": source['name']})
            print(f"[{source['name']}] Current links: {count}")
            
            if count < 5 and self.config['logic'].get('use_sitemap', False):
                print(f"[{source['name']}] Seems empty or stuck. Starting Bootstrap (Sitemap/RSS)...")
                bootstrap_tasks.append(self.process_source_bootstrap(source))
        
        if bootstrap_tasks:
            await asyncio.gather(*bootstrap_tasks)
            print("--- Bootstrap Finished ---")

        print("Starting main crawler loop...")
        
        while not self.stop_signal:
            if self.current_count >= self.max_documents:
                print(f"Goal reached! Stopping.")
                self.stop_signal = True
                break

            batch_size = self.config['logic']['concurrency'] * 2
            batch = await self.db.get_batch_to_crawl(batch_size=batch_size)
            
            if not batch:
                pending_check = await self.db.count_pending()
                if pending_check == 0:
                     print("Queue fully empty. Stopping.")
                     break
                print("Queue temporarily empty (waiting for next checks)...")
                await asyncio.sleep(5)
                continue
            
            tasks = [asyncio.create_task(self.process_page(doc)) for doc in batch]
            await asyncio.gather(*tasks)
            
        print("Crawler finished.")