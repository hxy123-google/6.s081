// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define HASH(blockno) (blockno % NBUCKET)
struct {
 // struct spinlock lock;
  struct buf buf[NBUF];
  struct buf head[NBUCKET];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  int size;
  struct spinlock lock;
  struct spinlock headlock[NBUCKET];
  struct spinlock buflock;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.buflock, "buflock");
  initlock(&bcache.lock, "bcache");
  // Create linked list of buffers
  for(int i=0;i<NBUCKET;i++){
     initlock(&bcache.headlock[i], "bcache_bucket");
  }
  for(b=bcache.buf;b<bcache.buf+NBUF;b++){
    initsleeplock(&b->lock, "buffer");
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *lru=0;
  //search buf in own bucket
  acquire(&bcache.headlock[HASH(blockno)]);
  // Is the block already cached?
  for(b = bcache.head[HASH(blockno)].next; b ; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.headlock[HASH(blockno)]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  int min_time = 0x8fffffff;
  for(b = bcache.head[HASH(blockno)].next; b ; b = b->next){
    //find the least used block
    if(b->refcnt==0&&b->timestamp<min_time){
      // b->dev = dev;
      // b->blockno = blockno;
      // b->valid = 0;
      // b->refcnt = 1;
      lru=b;
      min_time = b->timestamp;
      // release(&bcache.headlock[HASH(blockno)]);
      // acquiresleep(&b->lock);

    }
    
  }
  if(lru){
    lru->dev=dev;
    lru->blockno = blockno;
    lru->valid = 0;
    lru->refcnt = 1;
    lru->timestamp = ticks;
    release(&bcache.headlock[HASH(blockno)]);
    acquiresleep(&lru->lock);
    return lru;
  }
  release(&bcache.headlock[HASH(blockno)]);
  //search in buf
  int is_buf=0;//判断是否在缓冲区里面找的
  acquire(&bcache.buflock);
  if(bcache.size<NBUF){
    is_buf=1;
    lru=&bcache.buf[bcache.size++];
    lru->dev=dev;
    lru->blockno = blockno;
    lru->valid = 0;
    lru->refcnt = 1;
    lru->timestamp = ticks;
  }
  release(&bcache.buflock);

  //search in other chain
  int is_find=0;
  if(!is_buf){
    is_find=0;
    acquire(&bcache.lock);
    while(!is_find){
      min_time = 0x8fffffff;
      for(b=bcache.buf;b<bcache.buf+NBUF;b++){
        if(b->refcnt==0&&b->timestamp<min_time){
          lru=b;
          min_time=b->timestamp;
        }
      }
      if(lru){
        int rid=HASH(lru->blockno);
        acquire(&bcache.headlock[rid]);
        if(lru->refcnt!=0){
          release(&bcache.headlock[rid]);
          continue;
        }
        is_find=1;
        struct buf* pre=&bcache.head[rid];
        struct buf* p=bcache.head[rid].next;
        while(p!=lru){
          p=p->next;
          pre=pre->next;
        }
        pre->next=p->next;
        release(&bcache.headlock[rid]);
        release(&bcache.lock);
        
      }else{
        release(&bcache.lock);
        break;
      }
    }
  }
  if(is_buf||is_find){
    if(lru){
      acquire(&bcache.headlock[HASH(blockno)]);
      lru->next=bcache.head[HASH(blockno)].next;
      bcache.head[HASH(blockno)].next=lru;
      lru->dev=dev;
      lru->blockno = blockno;
      lru->valid = 0;
      lru->refcnt = 1;
      lru->timestamp = ticks;
      release(&bcache.headlock[HASH(blockno)]);
      acquiresleep(&lru->lock);
      return lru;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.headlock[HASH(b->blockno)]);
  b->refcnt--;
  release(&bcache.headlock[HASH(b->blockno)]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.headlock[HASH(b->blockno)]);
  b->refcnt++;
  release(&bcache.headlock[HASH(b->blockno)]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.headlock[HASH(b->blockno)]);
  b->refcnt--;
  release(&bcache.headlock[HASH(b->blockno)]);
}


