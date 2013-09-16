
#include "ProteinAnalysis.hpp"
#include "Viewer/SlotViewer.hpp"
#include <algorithm>
#include <thread>
#include <iterator>
#include <iostream>

typedef std::unordered_multimap<ProteinAnalysis::Bucket, AtomPtr,
    ProteinAnalysis::Bucket> BucketMap;
typedef std::unordered_multimap<AtomPtr, AtomPtr> NeighborList;
typedef std::unordered_set<AtomPtr> AtomGroup;

ProteinAnalysis::ProteinAnalysis(const TrajectoryPtr& trajectory) :
    trajectory_(trajectory)
{}



void ProteinAnalysis::fixProteinSplits()
{
    std::cout << "[concurrent] Analyzing protein for splits..." << std::endl;

    BucketMap bucketMap = getBucketMap();
    NeighborList neighborList = getNeighborList(bucketMap);
/*
    auto firstAtom = trajectory_->getTopology()->getAtoms()[0];
    auto group = getGroupFrom(firstAtom, neighborList);*/
}



BucketMap ProteinAnalysis::getBucketMap()
{
    std::cout << "[concurrent] hashing atoms into cube buckets... ";
    BucketMap bucketMap;

    const auto ATOMS = trajectory_->getTopology()->getAtoms();
    auto snapshotZero = trajectory_->getSnapshot(0);

    //hash each atom into some bucket according to its position
    for_each (ATOMS.begin(), ATOMS.end(),
        [&](const AtomPtr& atom)
        {
            auto pos = snapshotZero[atom];
            Bucket b;
            b.x = (int)(pos.x / BOND_LENGTH);
            b.y = (int)(pos.y / BOND_LENGTH);
            b.z = (int)(pos.z / BOND_LENGTH);

            bucketMap.insert(BucketMap::value_type(b, atom));
        }
    );

    std::cout << "done." << std::endl;
    return bucketMap;
}



NeighborList ProteinAnalysis::getNeighborList(const BucketMap& bucketMap)
{
    std::cout << "[concurrent] Computing neighbor list for each atom... ";
    NeighborList neighborList;

    //iterate through each bucket
        //calculate the 27 buckets
        //then iterate through the atoms in that bucket

    int count = 0;
    //iterate through buckets
    auto bucketIterator = bucketMap.begin();
    while (bucketIterator != bucketMap.end())
    {
        auto currentBucket = bucketIterator->first;
        auto currBucketRange = bucketMap.equal_range(currentBucket);

        //collect a list of iterators to surrounding buckets
        typedef BucketMap::const_iterator Iterator;
        std::vector<std::pair<Iterator, Iterator>> neighboringBuckets;
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                for (int z = -1; z <= 1; z++)
                {
                    Bucket neighbor;
                    neighbor.x = currentBucket.x + x;
                    neighbor.y = currentBucket.y + y;
                    neighbor.z = currentBucket.z + z;
                    neighboringBuckets.push_back(bucketMap.equal_range(neighbor));
                }
            }
        }

        //loop through all atoms in current bucket
        for_each (currBucketRange.first, currBucketRange.second,
            [&](const std::pair<Bucket, AtomPtr>& currAtomPair)
        {
            auto currentAtom = currAtomPair.second;
            count++;
            std::cout << count << std::endl;
            //iterate through surrounding buckets
            for_each (neighboringBuckets.begin(), neighboringBuckets.end(),
                [&](const std::pair<Iterator, Iterator>& neighborBucket)
            {
                //iterate through atoms in that neighboring bucket
                for_each (neighborBucket.first, neighborBucket.second,
                    [&](const std::pair<Bucket, AtomPtr>& near)
                {
                    //test the nearby atom
                    auto nearbyAtom = near.second;
                    if (isWithinBondDistance(currentAtom, nearbyAtom))
                    {
                        //if it is within range, add it
                        auto pair = NeighborList::value_type(currentAtom, nearbyAtom);
                        neighborList.insert(pair);
                    }
                });
            });
        });

        bucketIterator = currBucketRange.second;
    }
/*
        for_each (neighboringBuckets.begin(), neighboringBuckets.end(),
            [&](int iterators)
            {
                auto nearbyAtom = near.second;

                if (isWithinBondDistance(atom, nearbyAtom))
                {
                    auto pair = NeighborList::value_type(atom, nearbyAtom);
                    neighborList.insert(pair);
                }
            }
        );*
    }
/*
    int index = 0;
    for_each (bucketMap.begin(), bucketMap.end(),
        [&](const std::pair<Bucket, AtomPtr>& current)
    {
        auto bucket = current.first;
        auto atom = current.second;

        std::cout << index << std::endl;
        index++;

        //scan surrounding 27 buckets
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                for (int z = -1; z <= 1; z++)
                {
                    Bucket neighborBucket;
                    neighborBucket.x = bucket.x + x;
                    neighborBucket.y = bucket.y + y;
                    neighborBucket.z = bucket.z + z;

                    auto contents = bucketMap.equal_range(neighborBucket);
                    for_each (contents.first, contents.second,
                        [&] (const std::pair<Bucket, AtomPtr>& near)
                    {
                        auto nearbyAtom = near.second;

                        if (isWithinBondDistance(atom, nearbyAtom))
                        {
                            auto pair = NeighborList::value_type(atom, nearbyAtom);
                            neighborList.insert(pair);
                        }
                    });
                }
            }
        }
    });
*/
    std::cout << " done." << std::endl;
    return neighborList;
}



AtomGroup ProteinAnalysis::getGroupFrom(const AtomPtr& startAtom,
                                        const NeighborList& neighborList
)
{
    std::cout << "[concurrent] Recursing through atom group... ";

    AtomGroup group;
    recurseThroughGroup(startAtom, neighborList, group);

    std::cout << " done." << std::endl;
    return group;
}



void ProteinAnalysis::recurseThroughGroup(const AtomPtr& atom,
    const NeighborList& neighborList, AtomGroup& visitedAtoms
)
{
    auto neighbors = neighborList.equal_range(atom);
    for_each (neighbors.first, neighbors.second,
        [&](const NeighborList::value_type& x)
        {
            if (visitedAtoms.count(atom) == 0)
            {
                visitedAtoms.insert(atom);
                recurseThroughGroup(x.second, neighborList, visitedAtoms);
            }
        }
    );
}



bool ProteinAnalysis::isWithinBondDistance(const AtomPtr& a, const AtomPtr& b)
{
    auto snapshotZero = trajectory_->getSnapshot(0);
    auto positionA = snapshotZero[a];
    auto positionB = snapshotZero[b];

    float dist = SlotViewer::getDotProduct(positionA, positionB);
    return dist >= (2 * 2);
}
