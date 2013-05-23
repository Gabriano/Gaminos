/* file: "queue.h" */

/*
 * Copyright (c) 2001 by Marc Feeley and Université de Montréal, All
 * Rights Reserved.
 *
 * Revision History
 * 02 Nov 01  initial version (Marc Feeley)
 */

/*
 * This file is normally included with "#include" to generate a set of
 * queue routines.  It should be included once for every set of queue
 * routines that is needed.  In C++ we would normally use templates to
 * achieve this goal, but in C the "#include" approach is required.
 * Just before including the file the following macros must be
 * defined:
 *
 *   NODETYPE                the base type for the queue and its elements
 *   QUEUETYPE               the NODETYPE subtype for the queue
 *   ELEMTYPE                the NODETYPE subtype for the elements of the queue
 *   NAMESPACE_PREFIX(name)  creates a prefixed name to avoid name clashes
 *
 *   BEFORE(elem1,elem2)      are elem1 and elem2 in order?
 *   NEXT(node)               the next node
 *   NEXT_SET(node,next_node) set the next node
 *   PREV(node)               the previous node
 *   PREV_SET(node,prev_node) set the previous node
 */

/*---------------------------------------------------------------------------*/

#ifdef USE_DOUBLY_LINKED_LIST

/*
 * Priority queue implementation using doubly-linked lists.
 */


inline void NAMESPACE_PREFIX(init) (QUEUETYPE* queue)
{
  NEXT_SET (CAST(NODETYPE*,queue), CAST(NODETYPE*,queue));
  PREV_SET (CAST(NODETYPE*,queue), CAST(NODETYPE*,queue));
}


inline void NAMESPACE_PREFIX(detach) (ELEMTYPE* elem)
{
  NEXT_SET (CAST(NODETYPE*,elem), CAST(NODETYPE*,elem));
  PREV_SET (CAST(NODETYPE*,elem), CAST(NODETYPE*,elem));
}


inline ELEMTYPE* NAMESPACE_PREFIX(head) (QUEUETYPE* queue)
{
  NODETYPE* head = NEXT (CAST(NODETYPE*,queue));

  if (head != CAST(NODETYPE*,queue))
    return CAST(ELEMTYPE*,head);

  return NULL;
}


inline void NAMESPACE_PREFIX(insert) (ELEMTYPE* elem, QUEUETYPE* queue)
{
  NODETYPE* node2 = CAST(NODETYPE*,queue);
  NODETYPE* node1 = PREV (node2);

  while (node1 != CAST(NODETYPE*,queue) &&
         BEFORE (elem, CAST(ELEMTYPE*,node1)))
    {
      node2 = node1;
      node1 = PREV (node2);
    }

  // insert elem between node1 and node2

  NEXT_SET (node1, CAST(NODETYPE*,elem));
  PREV_SET (CAST(NODETYPE*,elem), node1);

  NEXT_SET (CAST(NODETYPE*,elem), node2);
  PREV_SET (node2, CAST(NODETYPE*,elem));
}


inline void NAMESPACE_PREFIX(remove) (ELEMTYPE* elem)
{
  NODETYPE* prev_node = PREV (CAST(NODETYPE*,elem));
  NODETYPE* next_node = NEXT (CAST(NODETYPE*,elem));

  NEXT_SET (prev_node, next_node);
  PREV_SET (next_node, prev_node);
}


#endif


/*---------------------------------------------------------------------------*/
