/*
 * File: allocator.c
 * Authors: Charlie Dektar and Jonathan Y Engel
 * ----------------------
 * A fairly dece allocator. Uses an explicit list of freed headers to
 * search for free blocks for allocating in a find-first-fit algorithm,
 * and coalesces freed blocks upon free and realloc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "allocator.h"
#include "segment.h"

// Heap blocks are required to be aligned to 8-byte boundary
#define ALIGNMENT 8
#define MAX 1 << 30 //the maximum value the allocator allows 
#define STARTING_PAGES 41
#define FREE_MASK 0x80 //free bit
#define PREV_FREE_MASK 0x40 //previous block is free bit
#define PREV_EIGHT_MASK 0x20 //previous block is 8 free bit
#define EST_MEDIAN 10000 //good value

typedef struct {
    size_t payloadsz;
    int housekeeping; //contains crucial housekeeping information
} headerT;

static int Num_Pages; //keeps track of number of pages added to the heap
static void* Heap_Start; //keeps track of start address of heap, used for
			 //iteration and pointer arithmetic
static int Num_Headers; //keeps track of the number of headers used in the
			//heap, primarily used for validate_heap proofreading
static headerT* Last_Header; //Keeps track of the last header in the heap (by
			  //address)Used for heuristic search purposes and
			  //validate_heap. Last_Header should always be free.
static void** Small_Side_Linked; //A FILO organized linked-list that keeps track
			   //of all of the freed chunks of data
static void** Big_Side_Linked;




// Very efficient bitwise round of sz up to nearest multiple of mult
// does this by adding mult-1 to sz, then masking off the
// the bottom bits to compute least multiple of mult that is
// greater/equal than sz, this value is returned
// NOTE: mult has to be power of 2 for the bitwise trick to work!
static inline size_t roundup(size_t sz, int mult)
{
  return (sz + mult-1) & ~(mult-1);
}


// Given a pointer to start of payload, simply back up
// to access its block header
static inline headerT *hdr_for_payload(void *payload)
{
  return (headerT *)((char *)payload - sizeof(headerT));
}

//Function: hdr_for_back_link
//----------------------------
//Given the back link of a freed payload (ie, when iterating from
//Big_Side_Linked), returns the header.
headerT* hdr_for_back_link (void** back_link) {
  return (headerT*) ((char*)back_link - sizeof(headerT) - sizeof(void**));
}


// Given a pointer to block header, advance past
// header to access start of payload
static inline void *payload_for_hdr(headerT *header)
{
  return (char *)header + sizeof(headerT);
}

//Function: end_payload
//----------------------
//Returns the location of the end of the payload of a given header
void* end_payload(headerT* header) {
  void* end_payload = (char*)header + sizeof(headerT) + header->payloadsz;
  return end_payload;
}

//Function: check_edge
//--------------------
//Used to determine if the allocation of the given payload returns
//leaves insufficient space for an additional freed header before adding
//an additional heap segment. Returns false if there is not enough space
//and true if there is space left.
bool check_edge (headerT *curr_header) {
  void* next_offset = end_payload (curr_header);
  if ((char*)next_offset >= ((char*)Heap_Start + (Num_Pages * PAGE_SIZE) -
			     sizeof(headerT))  ) {
    return false;
  } 
  return true;
}

//Function: next_free_header
//-------------------------
//Returns the headerT* header of the next item in the linked list of free chunks
headerT* next_free_header (void** current_link) {
  return hdr_for_payload(*current_link);
}

headerT* prev_free_header (void** current_link) {
  return hdr_for_back_link(*current_link);
}

//Function: back_link_for_hdr
//---------------------------
//Given a pointer to a headerT header within the heap, returns the void**
//pointer that is a node in the doubly linked list of free blocks. The
//linked list is doubly linked, the back_link is offset four bytes from
//the headerT, so this function returns that second memory address as a void**.
void** back_link_for_hdr (headerT* header) {
  return (void**) ((char*)header + sizeof(headerT) + sizeof(void**) ) ;
}

//Function: front_from_back
//--------------------------
//Given a void** pointer that is the back link (a node in the doubly
//linked list going backwards that is found after the node going forwards
//in the heap), returns the void** pointer that was immediately before the
//given back_link. Those four byte cast as a void** are a node in the
//doubly linked free list going forward.
void** front_from_back (void** back_link) {
  return (void**) ((char*)back_link - sizeof(void*));
}

//Function: back_from_front
//--------------------------
//Given a void** pointer that is the front link (a node in the doubly
//linked list going forwards that is found immediately after the header),
//returns the void** pointer that was immediately after the front
//link. That pointer is a node in the doubly linked free list going
//backwards. 
void** back_from_front (void** front_link) {
  return (void**) (  (char*)front_link + sizeof(void*));
}

//Function: append_to_linked
//---------------------------
//Given a newly freed header, it adds it to the correct side of the doubly
//linked list
void append_to_linked (headerT* header) {
  if (Small_Side_Linked == NULL ) { //nothing in free linked list
    Small_Side_Linked = (void**)payload_for_hdr(header);
    *Small_Side_Linked = NULL;
    Big_Side_Linked = back_link_for_hdr (header);
    *Big_Side_Linked = NULL;
    
  } else {
    if (header->payloadsz < EST_MEDIAN) {
      *(void**)payload_for_hdr(header) = Small_Side_Linked;
      *back_link_for_hdr (header) = NULL; 
      *(back_from_front (Small_Side_Linked)) = (void**)back_link_for_hdr(header);
      Small_Side_Linked = (void**)payload_for_hdr(header);
      
    } else {
      *back_link_for_hdr(header) = Big_Side_Linked;
      *(void**)payload_for_hdr(header) = NULL;
      *(front_from_back (Big_Side_Linked)) = payload_for_hdr (header);
      Big_Side_Linked = back_link_for_hdr(header);
      
    }
  }
}

//Function: remove_from_linked
//----------------------------
//Given a header, it removes it from the doubly linked free list. Is
//called when allocating a new block and when coalescing freed blocks

void remove_from_linked (headerT* header) {
  if ((char*)front_from_back(Big_Side_Linked) == (char*)Small_Side_Linked
      ){ //CASE: Only one item in free list
    Big_Side_Linked = NULL;
    Small_Side_Linked = NULL;
    return;
  } 
  
  void** back_link = back_link_for_hdr (header);
  void** front_link = (void**)payload_for_hdr (header);
  
  if (*front_link == NULL) { //Case: Reached the end of the list starting from the front
    Big_Side_Linked = *back_link;
    *front_from_back(Big_Side_Linked) = NULL;
  } else if (*back_link == NULL) { //Case: Reached the end of the list
                                   //    starting from the back
    Small_Side_Linked = *front_link;
    *back_from_front(Small_Side_Linked) = NULL;
  } else { //Case: Somewhere in the middle of the list
    *back_from_front(*front_link) = *back_link;  
    *front_from_back(*back_link) = *front_link; 
  }
}

//Function: make_footer
//---------------------
//Called on a newly free'd or recently modified free header of payloadsize
//> 8 to turn the last four bytes of the payload into a footer pointing
//back at the beginning of the payload
void make_footer(headerT* header) {
  void** footer = (void**)((char*)end_payload(header) - sizeof(void**));
  *footer = payload_for_hdr(header);
}

//Function: set_next_mask
//-----------------------
//Given a header, it sets the housekeeping mask of the next header to reflect
//what the previous header's free value is. It is called in the event that
//the header is free; setting the mask value for headers upon allocation
//of the previous header is handled elsewhere, as unifying it dropped throughput.
void set_next_mask (headerT* header) {
  headerT* next_Header = (headerT*)((char*)header + sizeof(headerT) + header->payloadsz);
  if ((char*)header != (char*)Last_Header && Last_Header != NULL) {
    if (header->payloadsz > ALIGNMENT ) {
      next_Header->housekeeping &= (~PREV_EIGHT_MASK);
      next_Header->housekeeping |= PREV_FREE_MASK;;
      make_footer(header);
    } else if (header->payloadsz == ALIGNMENT) {
      next_Header->housekeeping &= ~PREV_FREE_MASK;
      next_Header->housekeeping |= PREV_EIGHT_MASK;
    }
  }  
}


/*
 * Function: set_free_value
 *--------------------------
 * Given a free value of 1 or 0, sets the free value of a header and the
 * next header accordingly.
 */
void set_free_value (int is_free, headerT *header) {
  headerT* next_Header = (headerT*)end_payload(header);
  
  if (is_free == 0) {
    remove_from_linked(header);
    header->housekeeping &= ~FREE_MASK;
    next_Header->housekeeping &= FREE_MASK; 
  } else if (is_free == 1) {
    append_to_linked(header);
    header->housekeeping |= FREE_MASK;
    
    if ((char*)next_Header <= (char*)Last_Header) {
      set_next_mask(header);
    }
  }
}


/* Function: make_next_header
 * --------------------------
 * Given a header, makes a new header to go in the
 * remaining free space after the payloadsz
 *
 */
void make_next_header (headerT *header, int free_space) {
  
  headerT *nextHeader = (headerT*)(end_payload(header));
  
  if (!check_edge(header)) { //Case: previously allocated chunk is at the
                             //end of the heap
    size_t npages = roundup(sizeof(headerT), PAGE_SIZE)/PAGE_SIZE;
    Num_Pages += npages;
    void* new_page = (extend_heap_segment(npages));
    int diff = (char*)new_page - (char*)nextHeader;
    header->payloadsz += diff;
    nextHeader = (headerT*)new_page;
    free_space  = npages * PAGE_SIZE;
    
  } else if (free_space < ALIGNMENT*2) { //Case: allocating to freed block in the
                                         //    middle of the heap, no need for a new header, exit without doing anything
    header->payloadsz += free_space;
    return;
  }    
  
  nextHeader->payloadsz = free_space;
  nextHeader->payloadsz -= sizeof(headerT);
  Num_Headers++;
  
  if ((char*)nextHeader > (char*)Last_Header) {
    Last_Header = nextHeader;
  }

  nextHeader->housekeeping &= 0;
  set_free_value(1, nextHeader);
}

//Function: assign_memory
//----------------------
//Given a header and a requested size, returns the payload for the header
//to be used as the output of a mymalloc call (ie it returns the final
//location of allocated memory in the heap
void* assign_memory (size_t requestedsz, headerT* header) {
  int free_space = header->payloadsz - requestedsz; //Because this header
  //was previously free, it calculates (any) remaining free space so that
  //make_next_header knows if to make a new header following this one and
  //how much payload (free space) to assign it
  header->payloadsz = requestedsz;
  make_next_header (header, free_space);
  
  set_free_value (0, header);
  
  return payload_for_hdr(header);
}


//Function: initialize_pages
//--------------------------
//Extends the heap segment and adds a free header to the beginning of the
//new free page. This header is returned
headerT* initialize_pages (int npages) {
  headerT* header = extend_heap_segment(npages);                                     
  header->payloadsz = (npages * PAGE_SIZE) - sizeof(headerT);
  
  Num_Headers++;
  Last_Header = header;
  set_free_value (1, header);
  return header;
}

//Function: get_new_page
//----------------------
//Calls initialize_pages if this is the first page of the heap, otherwise
//extends the heap segment and tacks that on to the last free header
void* get_new_page (size_t requestedsz) {
  
  size_t npages = roundup(requestedsz + sizeof(headerT),PAGE_SIZE)/PAGE_SIZE;  
  headerT *header;
  
  if (Small_Side_Linked == NULL) {
    if (npages < STARTING_PAGES) {
      npages = STARTING_PAGES;
    }
    Num_Pages += npages;
    header = initialize_pages(npages);
  } else {
    
    Num_Pages += npages;
    header = Last_Header;
    header->payloadsz += (npages * PAGE_SIZE);
    extend_heap_segment(npages);
  } 
  return assign_memory(requestedsz, header);
}



//Function: iterate_to_next
//--------------------------
//Given a header, iterates to the next header and returns a pointer to it
headerT* iterate_to_next (headerT* curr_header ) {
  void* next_offset = end_payload (curr_header);
  if ((char*)next_offset > ((char*)Heap_Start + (Num_Pages * PAGE_SIZE) -
			    sizeof(headerT)) ) return NULL;
  curr_header = ( (headerT*)next_offset );
  return curr_header;
}

//Function: find_free
//------------------
//Does a find first fit search of the free linked list to find a header
//with sufficient payloadsize to allocate the newly requested data. Based
//on the size requested, it will start at either the small end or big end
//of the linked list.
headerT* find_free (size_t requestedsz) {
  
  bool small = false;
  headerT* header;
  if (requestedsz < EST_MEDIAN) {
    
    small = true;
    header = hdr_for_payload(Small_Side_Linked);
  } else {
    
    header = hdr_for_back_link(Big_Side_Linked);
  }
  void** current;
  if (small) {
    current = (void**)payload_for_hdr(header);
  } else {
    current = (void**)back_link_for_hdr(header);
  }
  while (header->payloadsz < requestedsz) {
    
    if (*current == NULL) return NULL;
    if (small) {
      header = next_free_header(current);
    } else {
      header = prev_free_header(current);
    }
    current = *current;
  }
  return header;
}

//Function: malloc_memory
//----------------------
//Calls find_free to attempt to find a freed header of a given size, then
//either calls assign_memory to assign memory to it or get_new_page to
//request additional memory from the OS
void *malloc_memory(size_t requestedsz) {
  headerT* first_fit = find_free(requestedsz);
  if (first_fit == NULL) {
    return get_new_page (requestedsz);
  } else {  
    return assign_memory(requestedsz, first_fit);
  }
}

//Function: round_to_alignment
//------------------------------
//Rounds requested sizes to the alignment constant. Also enforces the max
//size for requested memory
size_t round_to_alignment(size_t requestedsz) {
  if (requestedsz % ALIGNMENT != 0) {
    requestedsz += (ALIGNMENT - (requestedsz % ALIGNMENT));
  }
  if (requestedsz < ALIGNMENT) {
    requestedsz = ALIGNMENT;
  }
  if (requestedsz > MAX) {
    requestedsz = MAX;
  }
  return requestedsz;
}

//Function: mymalloc
//-----------------
//Determines whether the heap exists, and then either calls to get more
//memory or to malloc the requested memory
void *mymalloc(size_t requestedsz)
{
  requestedsz = round_to_alignment(requestedsz);

  if (Num_Pages == 0) {
    return get_new_page (requestedsz);
  } else {
    return malloc_memory(requestedsz);
  }
}

//Function: coalesce_previous
//--------------------------
//Coalesces a free header with a previous free header
void coalesce_previous (headerT* header, void* prev_payload) {
  headerT* prev_header = hdr_for_payload(prev_payload);
  prev_header->payloadsz += header->payloadsz + sizeof(headerT);
  
  if ((char*)header == (char*)Last_Header) {
    Last_Header = prev_header;
  }
  
  set_next_mask(prev_header);			
  Num_Headers--;
}

//Function: myfree 
//-----------------
//Frees a header by changing its housekeeping value and adding it to the linked
//list of free headers. Also coalesces with next header and previous header and updates
//Last_Header accordingly
void myfree(void *ptr)
{
  headerT* header = hdr_for_payload (ptr);  

  headerT* next_header = iterate_to_next (header);
  if (next_header != NULL && ((next_header->housekeeping & FREE_MASK) ==
			      FREE_MASK) ) {
    //attempted to unify this with the coalesce function, but it dropped
    //  throughput considerably
    header->payloadsz += next_header->payloadsz + sizeof(headerT);
    remove_from_linked(next_header);
    Num_Headers--;
    if ((char*)next_header == (char*)Last_Header ) {
      Last_Header = header;
      
    } 
  } 
  void* prev_payload;
  if ( (header->housekeeping & PREV_FREE_MASK) == PREV_FREE_MASK) {
    prev_payload = *(void**)((char*)header - sizeof(void**));
    coalesce_previous(header, prev_payload);
  } else if ( (header->housekeeping & PREV_EIGHT_MASK) == PREV_EIGHT_MASK) {
    prev_payload = (char*)header - ALIGNMENT;
    coalesce_previous(header, prev_payload);
  } else {
    set_free_value (1, header); 
  }
}

//Function: myrealloc
//--------------------
//Determines whether memory needs to be moved, and then moves it accordingly
void *myrealloc(void *oldptr, size_t newsz)
{
  newsz = round_to_alignment(newsz);
  headerT* oldHeader = (hdr_for_payload(oldptr));
  int oldsz = oldHeader->payloadsz;

  if (oldHeader->payloadsz >= newsz) {
    return oldptr; 
  } else {
    headerT* nextHeader = iterate_to_next(oldHeader);
    if (nextHeader != NULL) {
      int potential_payload = oldHeader->payloadsz;
      
      if ((nextHeader->housekeeping & FREE_MASK) == FREE_MASK) {
	potential_payload += nextHeader->payloadsz + sizeof(headerT);
	int diff = potential_payload - newsz;

	if (potential_payload == newsz || diff  == ALIGNMENT) {
	  remove_from_linked (nextHeader);
	  oldHeader->payloadsz = potential_payload;
	  Num_Headers--;
	  
	  if (iterate_to_next (nextHeader) == NULL) {
	    Num_Pages++;
	    initialize_pages (1);
	  }
	  return oldptr;
	  
	} else if (potential_payload > newsz) {
	  remove_from_linked (nextHeader);
	  Num_Headers--;
	  oldHeader->payloadsz = newsz;
	  make_next_header(oldHeader, potential_payload - newsz);
	  return oldptr;
	}
      }
    }
  }
  void* newptr =  malloc_memory(newsz);
  memcpy(newptr, oldptr, oldsz);
  myfree(oldptr);
  return newptr;  
}

/* The responsibility of the myinit function is to set up the heap to
 * its initial, empty ready-to-go state. This will be called before
 * any allocation requests are made. The myinit function may also
 * be called later in program and is expected to wipe out the current
 * heap contents and start over fresh. This "reset" option is specificcally
 * needed by the test harness to run a sequence of scripts, one after another,
 * without restarting program from scratch.
 */
bool myinit()
{
  Num_Pages = 0;
  Heap_Start = init_heap_segment(0); // reset heap segment to empty, no
  // pages allocated
  Small_Side_Linked = NULL;
  Big_Side_Linked = NULL; 
  Num_Headers = 0;
  return true;
}



// validate_heap is your debugging routine to detect/report
// on problems/inconsistency within your heap data structures
bool validate_heap()
{
  headerT* iterator = Heap_Start;
  int count = 0;
  if (Num_Headers == 0) return true;
  void** current = Small_Side_Linked;
  headerT* header = hdr_for_payload(current);
  while ((char*)header != (char*)hdr_for_back_link(Big_Side_Linked)) {
    if ((int)(char*)current % 4 != 0) {
      return false;
    }
    header = hdr_for_payload(current);
    if ((header->housekeeping & FREE_MASK) != FREE_MASK) {
      printf("found non-free header\n");
      return false;
    }
    current = *current;
  }
  current = Big_Side_Linked;
  header = hdr_for_back_link(Big_Side_Linked);
  while ((char*)header != (char*)hdr_for_payload(Small_Side_Linked)) {
    if (current == NULL) {
      printf("linked list is broken\n");
      return false;
    }
    header = hdr_for_back_link(current);
    current = *current;
  }
  while (count < Num_Headers) {
    count++;
    iterator = iterate_to_next(iterator);
    if (iterator == NULL) break;
    if (Last_Header != NULL && (Last_Header->housekeeping & FREE_MASK) != FREE_MASK) {
      printf("Last header is not free\n");
      return false;
    }
    if ( count == Num_Headers - 1 && (char*)iterator != (char*)Last_Header) {
      printf("Last header is incorrect\n");
      printf("Current: %p, Payload: %d, Last: %p\n", iterator,  iterator->payloadsz, Last_Header);
      return false;
    }
  }
  if (count != Num_Headers) {
    printf("COULD NOT FIND ALL HEADERS\n");
    return false;
  }
  if (iterator != NULL) {
    printf("FOUND EXTRA HEADER\n");
    printf("free value %d, ", iterator->housekeeping);
    printf("size %d\n", iterator->payloadsz);
    return false;
  }
  return true;
}

