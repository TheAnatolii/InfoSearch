import sys
import yaml
import asyncio
from app.db import Database
from app.spider import Crawler

def load_config(path):
    with open(path, 'r') as f:
        return yaml.safe_load(f)

async def main():
    if len(sys.argv) < 2:
        print("Usage: python main.py config.yaml")
        sys.exit(1)
        
    config_path = sys.argv[1]
    config = load_config(config_path)
    
    print(f"Target: {config['logic']['max_documents']} documents.")
    
    db = Database(config)
    crawler = Crawler(config, db)
    
    try:
        await crawler.run()
    except KeyboardInterrupt:
        print("\nStopping by user request...")
    except Exception as e:
        print(f"\nCritical error: {e}")
    finally:
        await crawler.close()

if __name__ == "__main__":
    if sys.platform == 'win32':
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
        
    asyncio.run(main())