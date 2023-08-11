#include <common/logger.h>
#include <siberix/mm/page.hpp>
#include <siberix/mm/service.hpp>
#include <utils/alignment.h>

namespace Memory
{
    Logger& logger;
    BuddyAlloc::BuddyAlloc(MemoryManagement& service) {
        SizedArrayList<PageBlock, 256>& pageBlocks = service.getPageBlocks();
        for (int i = 0; i < 256; i++)
        {
            PageBlock& block = pageBlocks[i];
            u64 addrStart = alignUp(block.start, PAGE_MAX_SIZE;);
            u64 addrEnd = alignDown(block.end, PAGE_MAX_SIZE);

            u64 current = addrStart;
            while (current < addrEnd - PAGE_MAX_SIZE)
            {
                Pageframe* pages = service.addr2page(current);
                if (!pages)
                {
                    Logger::getAnonymousLogger().error("Section pages is not allocated!");
                    return;
                }

                memset(pages, 0, sizeof(Pageframe) * PAGES_PER_SET);
                pages->order = PAGE_MAX_ORDER;
                for (u64 i = 0; i < PAGES_PER_SET; i++)
                {
                    pages[i].flags = 1;
                    pages[i].first = pages;
                    pages[i].address = current + (i * PAGE_SIZE_4K);
                }
                pageList[PAGE_MAX_ORDER].add(reinterpret_cast<ListNode<Pageframe>*> pages);

                current += PAGE_MAX_SIZE;
            }
        }
    }

    BuddyAlloc::~BuddyAlloc() {}

    Pageframe* BuddyAlloc::allocatePhysMemory4K(u64 amount) {
        u8 order = getPageOrder(getPageAlignment(amount));
        if (order > PAGE_MAX_ORDER) { return nullptr; }

        Pageframe* page;
        u8 _order = order;
        LinkedList<Pageframe> list;
        while (_order <= PAGE_MAX_ORDER)
        {
            if (pageList[_order].m_count())
            {
                list = &(pageList[_order]);
                break;
            }
            _order++;
        }

        if (list != nullptr)
        {
            page = reinterpret_cast<Pageframe*>(list->extract());
            while (_order-- > order)
            { page = expand(page); }
            page->flags &= ~PFLAGS_FREE;
            return page;
        } else
        {
            logger.error("Cannot find any page with specific size. Out of Memory!");
            return nullptr;
        }
    }

    void BuddyAlloc::freePhysMemory4K(u64 address) {}

    void BuddyAlloc::freePhysMemory4K(Pageframe& page) {}

    void BuddyAlloc::markPagesUsed(u64 addressStart, u64 addressEnd) {}

    Pageframe* BuddyAlloc::expand(Pageframe& page) {
        if (page.flags & PFLAGS_KMEM)
        {
            logger.warn("Unable to expand page because it belongs to kernel allocator.");
            return nullptr;
        }

        /* Remove this page from upper order list */
        pageList[page.order].remove(reinterpret_cast<Pageframe*>(page));
        /* Decrease the order and find the lower tier list */
        LinkedList<Pageframe>& freelist = pageList[--page.order];

        Pageframe* newPage = reinterpret_cast<Pageframe*>(((uint64_t)&page) + ((1 << page.order) * sizeof(Pageframe)));

        newPage->order = page.order;
        newPage->flags |= PFLAGS_FREE;

        /* Insert this page into the lower tier free list */
        freelist.add(reinterpret_cast<ListNode<Pageframe>*>(newPage));

        return page;
    }

    Pageframe* BuddyAlloc::combine(Pageframe& page) {
        u32 osize = (1 << (page.order)) * sizeof(Pageframe);
        bool align = !(page->address % osize);

        Pageframe* newPage = reinterpret_cast<Pageframe*>(align ? &page + osize : &page - osize);
        if (newPage->flags & PFLAGS_FREE)
        {
            Pageframe* result = align ? page : newPage;
            pageList[newPage->order].remove((ListNode<Pageframe>*)newPage);
            pageList[++result->order].add((ListNode<Pageframe>*)result);

            return result;
        } else
        {
            pageList[page.order].add((ListNode<Pageframe>*)page);
            return nullptr;
        }
    }

    Pageframe* BuddyAlloc::combine(Pageframe& lpage, Pageframe& rpage) {}
} // namespace Memory