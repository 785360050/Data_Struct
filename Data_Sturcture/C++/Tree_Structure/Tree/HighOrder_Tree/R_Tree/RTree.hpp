#pragma once

// NOTE This file compiles under MSVC 6 SP5 and MSVC .Net 2003 it may not work on other compilers without modification.

// NOTE These next few lines may be win32 specific, you may need to modify them to compile on other platform
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <algorithm>

#define ASSERT assert // RTree uses ASSERT( condition )


#define RTREE_USE_SPHERICAL_VOLUME // Better split classification, may be slower on some systems

#include "Node.hpp"

/// \class RTree
/// Implementation of RTree, a multidimensional bounding rectangle tree.
/// Example usage: For a 3-dimensional tree use RTree<Object*, float, 3> myTree;
///
/// This modified, templated C++ version by Greg Douglas at Auran (http://www.auran.com)
///
/// DATATYPE Referenced data, should be int, void*, obj* etc. no larger than sizeof<void*> and simple type
/// ELEMTYPE Type of element such as int or float
/// NUMDIMS Number of dimensions such as 2 or 3
/// ELEMTYPEREAL Type of element that allows fractional and large values such as float or double, for use in volume calcs
///
/// NOTES: Inserting and removing data requires the knowledge of its constant Minimal Bounding Rectangle.
///        This version uses new/delete for nodes, I recommend using a fixed size allocator for efficiency.
///        Instead of using a callback function for returned results, I recommend and efficient pre-sized, grow-only memory
///        array similar to MFC CArray or STL Vector for returning search query result.
///
template <class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL = ELEMTYPE, int MAXNODES = 8, int MINNODES = MAXNODES / 2>
class RTree
{
protected:
	// using Node = Node<TMAXNODES>;
	using Node = _Node<DATATYPE, ELEMTYPE, NUMDIMS, MAXNODES>;
	using Rect = _Rect<ELEMTYPE, NUMDIMS>;
	using Branch = _Branch<DATATYPE, ELEMTYPE, NUMDIMS, MAXNODES>;
	using ListNode = _ListNode<DATATYPE, ELEMTYPE, NUMDIMS, MAXNODES>;

private:
	// const int MAXNODES = TMAXNODES; ///< Max elements in node
	// const int MINNODES = TMINNODES; ///< Min elements in node

	Node *m_root;					 ///< Root of tree
	ELEMTYPEREAL m_unitSphereVolume; ///< Unit sphere constant for required number of dimensions
public:
	/// Variables for finding a split partition
	struct PartitionVars
	{
		int m_partition[MAXNODES + 1];
		int m_total;
		int m_minFill;
		int m_taken[MAXNODES + 1];
		int m_count[2];
		Rect m_cover[2];
		ELEMTYPEREAL m_area[2];

		Branch m_branchBuf[MAXNODES + 1];
		int m_branchCount;
		Rect m_coverSplit;
		ELEMTYPEREAL m_coverSplitArea;
	};

private:
	bool InsertRectRec(Rect *a_rect, const DATATYPE &a_id, Node *a_node, Node **a_newNode, int a_level)
	{
		// Inserts a new data rectangle into the index structure.
		// Recursively descends tree, propagates splits back up.
		// Returns 0 if node was not split.  Old node updated.
		// If node was split, returns 1 and sets the pointer pointed to by
		// new_node to point to the new node.  Old node updated to become one of two.
		// The level argument specifies the number of steps up from the leaf
		// level to insert; e.g. a data rectangle goes in at level = 0.
		ASSERT(a_rect && a_node && a_newNode);
		ASSERT(a_level >= 0 && a_level <= a_node->m_level);

		int index;
		Branch branch;
		Node *otherNode;

		// Still above level for insertion, go down tree recursively
		if (a_node->m_level > a_level)
		{
			index = PickBranch(a_rect, a_node);
			if (!InsertRectRec(a_rect, a_id, a_node->m_branch[index].m_child, &otherNode, a_level))
			{
				// Child was not split
				a_node->m_branch[index].m_rect = CombineRect(a_rect, &(a_node->m_branch[index].m_rect));
				return false;
			}
			else // Child was split
			{
				a_node->m_branch[index].m_rect = NodeCover(a_node->m_branch[index].m_child);
				branch.m_child = otherNode;
				branch.m_rect = NodeCover(otherNode);
				return AddBranch(&branch, a_node, a_newNode);
			}
		}
		else if (a_node->m_level == a_level) // Have reached level for insertion. Add rect, split if necessary
		{
			branch.m_rect = *a_rect;
			branch.m_child = (Node *)a_id;
			// Child field of leaves contains id of data record
			return AddBranch(&branch, a_node, a_newNode);
		}
		else
		{
			// Should never occur
			ASSERT(0);
			return false;
		}
	}
	bool InsertRect(Rect *a_rect, const DATATYPE &a_id, Node **a_root, int a_level)
	{
		// Insert a data rectangle into an index structure.
		// InsertRect provides for splitting the root;
		// returns 1 if root was split, 0 if it was not.
		// The level argument specifies the number of steps up from the leaf
		// level to insert; e.g. a data rectangle goes in at level = 0.
		// InsertRect2 does the recursion.
		//
		ASSERT(a_rect && a_root);
		ASSERT(a_level >= 0 && a_level <= (*a_root)->m_level);
#ifdef _DEBUG
		for (int index = 0; index < NUMDIMS; ++index)
		{
			ASSERT(a_rect->m_min[index] <= a_rect->m_max[index]);
		}
#endif //_DEBUG

		Node *newRoot;
		Node *newNode;
		Branch branch;

		if (InsertRectRec(a_rect, a_id, *a_root, &newNode, a_level)) // Root split
		{
			newRoot = Node::AllocNode(); // Grow tree taller and new root
			newRoot->m_level = (*a_root)->m_level + 1;
			branch.m_rect = NodeCover(*a_root);
			branch.m_child = *a_root;
			AddBranch(&branch, newRoot, NULL);
			branch.m_rect = NodeCover(newNode);
			branch.m_child = newNode;
			AddBranch(&branch, newRoot, NULL);
			*a_root = newRoot;
			return true;
		}

		return false;
	}
	Rect NodeCover(Node *a_node)
	{
		// Find the smallest rectangle that includes all rectangles in branches of a node.
		ASSERT(a_node);

		int firstTime = true;
		Rect rect;

		for (int index = 0; index < a_node->m_count; ++index)
		{
			if (firstTime)
			{
				rect = a_node->m_branch[index].m_rect;
				firstTime = false;
			}
			else
			{
				rect = CombineRect(&rect, &(a_node->m_branch[index].m_rect));
			}
		}

		return rect;
	}
	bool AddBranch(Branch *a_branch, Node *a_node, Node **a_newNode)
	{
		// Add a branch to a node.  Split the node if necessary.
		// Returns 0 if node not split.  Old node updated.
		// Returns 1 if node split, sets *new_node to address of new node.
		// Old node updated, becomes one of two.
		ASSERT(a_branch);
		ASSERT(a_node);

		if (a_node->m_count < MAXNODES) // Split won't be necessary
		{
			a_node->m_branch[a_node->m_count] = *a_branch;
			++a_node->m_count;

			return false;
		}
		else
		{
			ASSERT(a_newNode);

			SplitNode(a_node, a_branch, a_newNode);
			return true;
		}
	}
	void DisconnectBranch(Node *a_node, int a_index)
	{
		// Disconnect a dependent node.
		// Caller must return (or stop using iteration index) after this as count has changed
		ASSERT(a_node && (a_index >= 0) && (a_index < MAXNODES));
		ASSERT(a_node->m_count > 0);

		// Remove element by swapping with the last element to prevent gaps in array
		a_node->m_branch[a_index] = a_node->m_branch[a_node->m_count - 1];

		--a_node->m_count;
	}
	int PickBranch(Rect *a_rect, Node *a_node)
	{
		// Pick a branch.  Pick the one that will need the smallest increase
		// in area to accomodate the new rectangle.  This will result in the
		// least total area for the covering rectangles in the current node.
		// In case of a tie, pick the one which was smaller before, to get
		// the best resolution when searching.
		ASSERT(a_rect && a_node);

		bool firstTime = true;
		ELEMTYPEREAL increase;
		ELEMTYPEREAL bestIncr = (ELEMTYPEREAL)-1;
		ELEMTYPEREAL area;
		ELEMTYPEREAL bestArea;
		int best;
		Rect tempRect;

		for (int index = 0; index < a_node->m_count; ++index)
		{
			Rect *curRect = &a_node->m_branch[index].m_rect;
			area = CalcRectVolume(curRect);
			tempRect = CombineRect(a_rect, curRect);
			increase = CalcRectVolume(&tempRect) - area;
			if ((increase < bestIncr) || firstTime)
			{
				best = index;
				bestArea = area;
				bestIncr = increase;
				firstTime = false;
			}
			else if ((increase == bestIncr) && (area < bestArea))
			{
				best = index;
				bestArea = area;
				bestIncr = increase;
			}
		}
		return best;
	}
	Rect CombineRect(Rect *a_rectA, Rect *a_rectB)
	{
		// Combine two rectangles into larger one containing both
		ASSERT(a_rectA && a_rectB);

		Rect newRect;

		for (int index = 0; index < NUMDIMS; ++index)
		{
			newRect.m_min[index] = std::min(a_rectA->m_min[index], a_rectB->m_min[index]);
			newRect.m_max[index] = std::max(a_rectA->m_max[index], a_rectB->m_max[index]);
		}

		return newRect;
	}
	void SplitNode(Node *a_node, Branch *a_branch, Node **a_newNode)
	{
		// Split a node.
		// Divides the nodes branches and the extra one between two nodes.
		// Old node is one of the new ones, and one really new one is created.
		// Tries more than one method for choosing a partition, uses best result.
		ASSERT(a_node);
		ASSERT(a_branch);

		// Could just use local here, but member or external is faster since it is reused
		PartitionVars localVars;
		PartitionVars *parVars = &localVars;
		int level;

		// Load all the branches into a buffer, initialize old node
		level = a_node->m_level;
		GetBranches(a_node, a_branch, parVars);

		// Find partition
		ChoosePartition(parVars, MINNODES);

		// Put branches from buffer into 2 nodes according to chosen partition
		*a_newNode = Node::AllocNode();
		(*a_newNode)->m_level = a_node->m_level = level;
		LoadNodes(a_node, *a_newNode, parVars);

		ASSERT((a_node->m_count + (*a_newNode)->m_count) == parVars->m_total);
	}
	ELEMTYPEREAL RectSphericalVolume(Rect *a_rect)
	{
		// The exact volume of the bounding sphere for the given Rect
		ASSERT(a_rect);

		ELEMTYPEREAL sumOfSquares = (ELEMTYPEREAL)0;
		ELEMTYPEREAL radius;

		for (int index = 0; index < NUMDIMS; ++index)
		{
			ELEMTYPEREAL halfExtent = ((ELEMTYPEREAL)a_rect->m_max[index] - (ELEMTYPEREAL)a_rect->m_min[index]) * 0.5f;
			sumOfSquares += halfExtent * halfExtent;
		}

		radius = (ELEMTYPEREAL)sqrt(sumOfSquares);

		// Pow maybe slow, so test for common dims like 2,3 and just use x*x, x*x*x.
		if (NUMDIMS == 3)
			return (radius * radius * radius * m_unitSphereVolume);
		else if (NUMDIMS == 2)
			return (radius * radius * m_unitSphereVolume);
		else
			return (ELEMTYPEREAL)(pow(radius, NUMDIMS) * m_unitSphereVolume);
	}
	ELEMTYPEREAL RectVolume(Rect *a_rect)
	{
		// Calculate the n-dimensional volume of a rectangle
		ASSERT(a_rect);

		ELEMTYPEREAL volume = (ELEMTYPEREAL)1;

		for (int index = 0; index < NUMDIMS; ++index)
		{
			volume *= a_rect->m_max[index] - a_rect->m_min[index];
		}

		ASSERT(volume >= (ELEMTYPEREAL)0);

		return volume;
	}
	ELEMTYPEREAL CalcRectVolume(Rect *a_rect)
	{
		// Use one of the methods to calculate retangle volume
#ifdef RTREE_USE_SPHERICAL_VOLUME
		return RectSphericalVolume(a_rect); // Slower but helps certain merge cases
#else										// RTREE_USE_SPHERICAL_VOLUME
		return RectVolume(a_rect); // Faster but can cause poor merges
#endif										// RTREE_USE_SPHERICAL_VOLUME
	}
	// Load branch buffer with branches from full node plus the extra branch.
	void GetBranches(Node *a_node, Branch *a_branch, PartitionVars *a_parVars)
	{
		ASSERT(a_node);
		ASSERT(a_branch);

		ASSERT(a_node->m_count == MAXNODES);

		// Load the branch buffer
		for (int index = 0; index < MAXNODES; ++index)
		{
			a_parVars->m_branchBuf[index] = a_node->m_branch[index];
		}
		a_parVars->m_branchBuf[MAXNODES] = *a_branch;
		a_parVars->m_branchCount = MAXNODES + 1;

		// Calculate rect containing all in the set
		a_parVars->m_coverSplit = a_parVars->m_branchBuf[0].m_rect;
		for (int index = 1; index < MAXNODES + 1; ++index)
		{
			a_parVars->m_coverSplit = CombineRect(&a_parVars->m_coverSplit, &a_parVars->m_branchBuf[index].m_rect);
		}
		a_parVars->m_coverSplitArea = CalcRectVolume(&a_parVars->m_coverSplit);

		a_node->Reset();
	}
	// Method #0 for choosing a partition:
	// As the seeds for the two groups, pick the two rects that would waste the
	// most area if covered by a single rectangle, i.e. evidently the worst pair
	// to have in the same group.
	// Of the remaining, one at a time is chosen to be put in one of the two groups.
	// The one chosen is the one with the greatest difference in area expansion
	// depending on which group - the rect most strongly attracted to one group
	// and repelled from the other.
	// If one group gets too full (more would force other group to violate min
	// fill requirement) then other group gets the rest.
	// These last are the ones that can go in either group most easily.
	void ChoosePartition(PartitionVars *a_parVars, int a_minFill)
	{
		ASSERT(a_parVars);

		ELEMTYPEREAL biggestDiff;
		int group, chosen, betterGroup;

		InitParVars(a_parVars, a_parVars->m_branchCount, a_minFill);
		PickSeeds(a_parVars);

		while (((a_parVars->m_count[0] + a_parVars->m_count[1]) < a_parVars->m_total) && (a_parVars->m_count[0] < (a_parVars->m_total - a_parVars->m_minFill)) && (a_parVars->m_count[1] < (a_parVars->m_total - a_parVars->m_minFill)))
		{
			biggestDiff = (ELEMTYPEREAL)-1;
			for (int index = 0; index < a_parVars->m_total; ++index)
			{
				if (!a_parVars->m_taken[index])
				{
					Rect *curRect = &a_parVars->m_branchBuf[index].m_rect;
					Rect rect0 = CombineRect(curRect, &a_parVars->m_cover[0]);
					Rect rect1 = CombineRect(curRect, &a_parVars->m_cover[1]);
					ELEMTYPEREAL growth0 = CalcRectVolume(&rect0) - a_parVars->m_area[0];
					ELEMTYPEREAL growth1 = CalcRectVolume(&rect1) - a_parVars->m_area[1];
					ELEMTYPEREAL diff = growth1 - growth0;
					if (diff >= 0)
					{
						group = 0;
					}
					else
					{
						group = 1;
						diff = -diff;
					}

					if (diff > biggestDiff)
					{
						biggestDiff = diff;
						chosen = index;
						betterGroup = group;
					}
					else if ((diff == biggestDiff) && (a_parVars->m_count[group] < a_parVars->m_count[betterGroup]))
					{
						chosen = index;
						betterGroup = group;
					}
				}
			}
			Classify(chosen, betterGroup, a_parVars);
		}

		// If one group too full, put remaining rects in the other
		if ((a_parVars->m_count[0] + a_parVars->m_count[1]) < a_parVars->m_total)
		{
			if (a_parVars->m_count[0] >= a_parVars->m_total - a_parVars->m_minFill)
			{
				group = 1;
			}
			else
			{
				group = 0;
			}
			for (int index = 0; index < a_parVars->m_total; ++index)
			{
				if (!a_parVars->m_taken[index])
				{
					Classify(index, group, a_parVars);
				}
			}
		}

		ASSERT((a_parVars->m_count[0] + a_parVars->m_count[1]) == a_parVars->m_total);
		ASSERT((a_parVars->m_count[0] >= a_parVars->m_minFill) &&
			   (a_parVars->m_count[1] >= a_parVars->m_minFill));
	}
	// Copy branches from the buffer into two nodes according to the partition.
	void LoadNodes(Node *a_nodeA, Node *a_nodeB, PartitionVars *a_parVars)
	{
		ASSERT(a_nodeA);
		ASSERT(a_nodeB);
		ASSERT(a_parVars);

		for (int index = 0; index < a_parVars->m_total; ++index)
		{
			ASSERT(a_parVars->m_partition[index] == 0 || a_parVars->m_partition[index] == 1);

			if (a_parVars->m_partition[index] == 0)
			{
				AddBranch(&a_parVars->m_branchBuf[index], a_nodeA, NULL);
			}
			else if (a_parVars->m_partition[index] == 1)
			{
				AddBranch(&a_parVars->m_branchBuf[index], a_nodeB, NULL);
			}
		}
	}
	// Initialize a PartitionVars structure.
	void InitParVars(PartitionVars *a_parVars, int a_maxRects, int a_minFill)
	{
		ASSERT(a_parVars);

		a_parVars->m_count[0] = a_parVars->m_count[1] = 0;
		a_parVars->m_area[0] = a_parVars->m_area[1] = (ELEMTYPEREAL)0;
		a_parVars->m_total = a_maxRects;
		a_parVars->m_minFill = a_minFill;
		for (int index = 0; index < a_maxRects; ++index)
		{
			a_parVars->m_taken[index] = false;
			a_parVars->m_partition[index] = -1;
		}
	}
	void PickSeeds(PartitionVars *a_parVars)
	{
		int seed0, seed1;
		ELEMTYPEREAL worst, waste;
		ELEMTYPEREAL area[MAXNODES + 1];

		for (int index = 0; index < a_parVars->m_total; ++index)
		{
			area[index] = CalcRectVolume(&a_parVars->m_branchBuf[index].m_rect);
		}

		worst = -a_parVars->m_coverSplitArea - 1;
		for (int indexA = 0; indexA < a_parVars->m_total - 1; ++indexA)
		{
			for (int indexB = indexA + 1; indexB < a_parVars->m_total; ++indexB)
			{
				Rect oneRect = CombineRect(&a_parVars->m_branchBuf[indexA].m_rect, &a_parVars->m_branchBuf[indexB].m_rect);
				waste = CalcRectVolume(&oneRect) - area[indexA] - area[indexB];
				if (waste > worst)
				{
					worst = waste;
					seed0 = indexA;
					seed1 = indexB;
				}
			}
		}
		Classify(seed0, 0, a_parVars);
		Classify(seed1, 1, a_parVars);
	}
	// Put a branch in one of the groups.
	void Classify(int a_index, int a_group, PartitionVars *a_parVars)
	{
		ASSERT(a_parVars);
		ASSERT(!a_parVars->m_taken[a_index]);

		a_parVars->m_partition[a_index] = a_group;
		a_parVars->m_taken[a_index] = true;

		if (a_parVars->m_count[a_group] == 0)
		{
			a_parVars->m_cover[a_group] = a_parVars->m_branchBuf[a_index].m_rect;
		}
		else
		{
			a_parVars->m_cover[a_group] = CombineRect(&a_parVars->m_branchBuf[a_index].m_rect, &a_parVars->m_cover[a_group]);
		}
		a_parVars->m_area[a_group] = CalcRectVolume(&a_parVars->m_cover[a_group]);
		++a_parVars->m_count[a_group];
	}
	// Delete a data rectangle from an index structure.
	// Pass in a pointer to a Rect, the tid of the record, ptr to ptr to root node.
	// Returns 1 if record not found, 0 if success.
	// RemoveRect provides for eliminating the root.
	bool RemoveRect(Rect *a_rect, const DATATYPE &a_id, Node **a_root)
	{
		ASSERT(a_rect && a_root);
		ASSERT(*a_root);

		Node *tempNode;
		ListNode *reInsertList = NULL;

		if (!RemoveRectRec(a_rect, a_id, *a_root, &reInsertList))
		{
			// Found and deleted a data item
			// Reinsert any branches from eliminated nodes
			while (reInsertList)
			{
				tempNode = reInsertList->m_node;

				for (int index = 0; index < tempNode->m_count; ++index)
				{
					InsertRect(&(tempNode->m_branch[index].m_rect),
							   tempNode->m_branch[index].m_data,
							   a_root,
							   tempNode->m_level);
				}

				ListNode *remLNode = reInsertList;
				reInsertList = reInsertList->m_next;

				remLNode->m_node->FreeNode();
				FreeListNode(remLNode);
			}

			// Check for redundant root (not leaf, 1 child) and eliminate
			if ((*a_root)->m_count == 1 && (*a_root)->Is_Branch())
			{
				tempNode = (*a_root)->m_branch[0].m_child;

				ASSERT(tempNode);
				a_root->FreeNode();
				*a_root = tempNode;
			}
			return false;
		}
		else
		{
			return true;
		}
	}
	// Delete a rectangle from non-root part of an index structure.
	// Called by RemoveRect.  Descends tree recursively,
	// merges branches on the way back up.
	// Returns 1 if record not found, 0 if success.
	bool RemoveRectRec(Rect *a_rect, const DATATYPE &a_id, Node *a_node, ListNode **a_listNode)
	{
		ASSERT(a_rect && a_node && a_listNode);
		ASSERT(a_node->m_level >= 0);

		if (a_node->Is_Branch()) // not a leaf node
		{
			for (int index = 0; index < a_node->m_count; ++index)
			{
				if (Overlap(a_rect, &(a_node->m_branch[index].m_rect)))
				{
					if (!RemoveRectRec(a_rect, a_id, a_node->m_branch[index].m_child, a_listNode))
					{
						if (a_node->m_branch[index].m_child->m_count >= MINNODES)
						{
							// child removed, just resize parent rect
							a_node->m_branch[index].m_rect = NodeCover(a_node->m_branch[index].m_child);
						}
						else
						{
							// child removed, not enough entries in node, eliminate node
							ReInsert(a_node->m_branch[index].m_child, a_listNode);
							DisconnectBranch(a_node, index); // Must return after this call as count has changed
						}
						return false;
					}
				}
			}
			return true;
		}
		else // A leaf node
		{
			for (int index = 0; index < a_node->m_count; ++index)
			{
				if (a_node->m_branch[index].m_child == (Node *)a_id)
				{
					DisconnectBranch(a_node, index); // Must return after this call as count has changed
					return false;
				}
			}
			return true;
		}
	}
	

	void FreeListNode(ListNode *a_listNode)
	{
		delete a_listNode;
	}
	// Decide whether two rectangles overlap.
	bool Overlap(Rect *a_rectA, Rect *a_rectB)
	{
		ASSERT(a_rectA && a_rectB);

		for (int index = 0; index < NUMDIMS; ++index)
		{
			if (a_rectA->m_min[index] > a_rectB->m_max[index] ||
				a_rectB->m_min[index] > a_rectA->m_max[index])
			{
				return false;
			}
		}
		return true;
	}
	// Add a node to the reinsertion list.  All its branches will later be reinserted into the index structure.
	void ReInsert(Node *a_node, ListNode **a_listNode)
	{
		ListNode *newListNode;

		// newListNode = AllocListNode();
		newListNode = ListNode::Alloc();
		newListNode->m_node = a_node;
		newListNode->m_next = *a_listNode;
		*a_listNode = newListNode;
	}

private:
	// Add a node to the reinsertion list.  All its branches will later be reinserted into the index structure.
	bool Search(Node *a_node, Rect *a_rect, int &a_foundCount, bool (*a_resultCallback)(DATATYPE a_data, void *a_context), void *a_context)
	{
		ASSERT(a_node);
		ASSERT(a_node->m_level >= 0);
		ASSERT(a_rect);

		if (a_node->Is_Branch()) // This is an internal node in the tree
		{
			for (int index = 0; index < a_node->m_count; ++index)
			{
				if (Overlap(a_rect, &a_node->m_branch[index].m_rect))
				{
					if (!Search(a_node->m_branch[index].m_child, a_rect, a_foundCount, a_resultCallback, a_context))
						return false; // Don't continue searching
				}
			}
		}
		else // This is a leaf node
		{
			for (int index = 0; index < a_node->m_count; ++index)
			{
				if (Overlap(a_rect, &a_node->m_branch[index].m_rect))
				{
					DATATYPE &id = a_node->m_branch[index].m_data;

					// NOTE: There are different ways to return results.  Here's where to modify
					if (&a_resultCallback)
					{
						++a_foundCount;
						if (!a_resultCallback(id, a_context))
							return false; // Don't continue searching
					}
				}
			}
		}

		return true; // Continue searching
	}
	void RemoveAllRec(Node *a_node)
	{
		ASSERT(a_node);
		ASSERT(a_node->m_level >= 0);

		if (a_node->Is_Branch()) // This is an internal node in the tree
		{
			for (int index = 0; index < a_node->m_count; ++index)
			{
				RemoveAllRec(a_node->m_branch[index].m_child);
			}
		}
		a_node->FreeNode();
	}
	void Reset()
	{
		// Delete all existing nodes
		RemoveAllRec(m_root);
	}
	void CountRec(Node *a_node, int &a_count)
	{
		if (a_node->Is_Branch()) // not a leaf node
		{
			for (int index = 0; index < a_node->m_count; ++index)
				CountRec(a_node->m_branch[index].m_child, a_count);
		}
		else // A leaf node
			a_count += a_node->m_count;
	}

public:
	RTree()
	{
		ASSERT(MAXNODES > MINNODES);
		ASSERT(MINNODES > 0);

		// We only support machine word size simple data type eg. integer index or object pointer.
		// Since we are storing as union with non data branch
		ASSERT(sizeof(DATATYPE) == sizeof(void *) || sizeof(DATATYPE) == sizeof(int));

		// Precomputed volumes of the unit spheres for the first few dimensions
		static constexpr float UNIT_SPHERE_VOLUMES[] = {
			0.000000f, 2.000000f, 3.141593f, // Dimension  0,1,2
			4.188790f, 4.934802f, 5.263789f, // Dimension  3,4,5
			5.167713f, 4.724766f, 4.058712f, // Dimension  6,7,8
			3.298509f, 2.550164f, 1.884104f, // Dimension  9,10,11
			1.335263f, 0.910629f, 0.599265f, // Dimension  12,13,14
			0.381443f, 0.235331f, 0.140981f, // Dimension  15,16,17
			0.082146f, 0.046622f, 0.025807f, // Dimension  18,19,20
		};

		m_root = Node::AllocNode();
		m_root->m_level = 0;
		m_unitSphereVolume = (ELEMTYPEREAL)UNIT_SPHERE_VOLUMES[NUMDIMS];
	}
	virtual ~RTree()
	{
		Reset(); // Free, or reset node memory
	}

	/// Insert entry
	/// \param a_min Min of bounding rect
	/// \param a_max Max of bounding rect
	/// \param a_dataId Positive Id of data.  Maybe zero, but negative numbers not allowed.
	void Insert(const ELEMTYPE a_min[NUMDIMS], const ELEMTYPE a_max[NUMDIMS], const DATATYPE &a_dataId)
	{
#ifdef _DEBUG
		for (int index = 0; index < NUMDIMS; ++index)
			ASSERT(a_min[index] <= a_max[index]);
#endif //_DEBUG

		Rect rect;

		for (int axis = 0; axis < NUMDIMS; ++axis)
		{
			rect.m_min[axis] = a_min[axis];
			rect.m_max[axis] = a_max[axis];
		}

		InsertRect(&rect, a_dataId, &m_root, 0);
	}

	/// Remove entry
	/// \param a_min Min of bounding rect
	/// \param a_max Max of bounding rect
	/// \param a_dataId Positive Id of data.  Maybe zero, but negative numbers not allowed.
	void Remove(const ELEMTYPE a_min[NUMDIMS], const ELEMTYPE a_max[NUMDIMS], const DATATYPE &a_dataId)
	{
#ifdef _DEBUG
		for (int index = 0; index < NUMDIMS; ++index)
		{
			ASSERT(a_min[index] <= a_max[index]);
		}
#endif //_DEBUG

		Rect rect;

		for (int axis = 0; axis < NUMDIMS; ++axis)
		{
			rect.m_min[axis] = a_min[axis];
			rect.m_max[axis] = a_max[axis];
		}

		RemoveRect(&rect, a_dataId, &m_root);
	}

	/// Find all within search rectangle
	/// \param a_min Min of search bounding rect
	/// \param a_max Max of search bounding rect
	/// \param a_searchResult Search result array.  Caller should set grow size. Function will reset, not append to array.
	/// \param a_resultCallback Callback function to return result.  Callback should return 'true' to continue searching
	/// \param a_context User context to pass as parameter to a_resultCallback
	/// \return Returns the number of entries found
	int Search(const ELEMTYPE a_min[NUMDIMS], const ELEMTYPE a_max[NUMDIMS], bool (*a_resultCallback)(DATATYPE a_data, void *a_context), void *a_context)
	{
#ifdef _DEBUG
		for (int index = 0; index < NUMDIMS; ++index)
		{
			ASSERT(a_min[index] <= a_max[index]);
		}
#endif //_DEBUG

		Rect rect;

		for (int axis = 0; axis < NUMDIMS; ++axis)
		{
			rect.m_min[axis] = a_min[axis];
			rect.m_max[axis] = a_max[axis];
		}

		// NOTE: May want to return search result another way, perhaps returning the number of found elements here.

		int foundCount = 0;
		Search(m_root, &rect, foundCount, a_resultCallback, a_context);

		return foundCount;
	}

	/// Remove all entries from tree
	void RemoveAll()
	{
		// Delete all existing nodes
		Reset();

		m_root = Node::AllocNode();
		m_root->m_level = 0;
	}

	/// Count the data elements in this container.  This is slow as no internal counter is maintained.
	int Count()
	{
		int count = 0;
		CountRec(m_root, count);
		return count;
	}


};
