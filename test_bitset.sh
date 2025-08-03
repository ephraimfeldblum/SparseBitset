#!/bin/bash

# Test script for Redis Bitset Module
# This script demonstrates the functionality of the bitset module

echo "=== Redis Bitset Module Test ==="
echo

# Check if Redis is running
if ! redis-cli ping > /dev/null 2>&1; then
    echo "Error: Redis server is not running. Please start Redis first."
    exit 1
fi

echo "Loading bitset module..."
redis-cli MODULE LOAD ./bitset.so

echo
echo "=== Basic Operations ==="

# Test basic add/exists operations
echo "Adding elements 1, 5, 10, 100 to 'testset':"
redis-cli BITS.ADD testset 1 5 10 100

echo "Checking if elements exist:"
echo "BITS.EXISTS testset 1: $(redis-cli BITS.EXISTS testset 1)"
echo "BITS.EXISTS testset 5: $(redis-cli BITS.EXISTS testset 5)"
echo "BITS.EXISTS testset 7: $(redis-cli BITS.EXISTS testset 7)"

echo "Size of testset: $(redis-cli BITS.SIZE testset)"

echo
echo "=== Min/Max Operations ==="
echo "Min element: $(redis-cli BITS.MIN testset)"
echo "Max element: $(redis-cli BITS.MAX testset)"

echo
echo "=== Successor/Predecessor Operations ==="
echo "Successor of 3: $(redis-cli BITS.SUCCESSOR testset 3)"
echo "Successor of 5: $(redis-cli BITS.SUCCESSOR testset 5)"
echo "Predecessor of 7: $(redis-cli BITS.PREDECESSOR testset 7)"
echo "Predecessor of 100: $(redis-cli BITS.PREDECESSOR testset 100)"

echo
echo "=== Array Conversion ==="
echo "All elements in testset:"
redis-cli BITS.TOARRAY testset

echo
echo "=== Set Operations ==="

# Create another set
echo "Creating another set 'testset2' with elements 5, 15, 100:"
redis-cli BITS.ADD testset2 5 15 100

echo "Union of testset and testset2 -> unionset:"
redis-cli BITS.UNION unionset testset testset2
echo "Elements in unionset:"
redis-cli BITS.TOARRAY unionset

echo "Intersection of testset and testset2 -> intersectset:"
redis-cli BITS.INTERSECT intersectset testset testset2
echo "Elements in intersectset:"
redis-cli BITS.TOARRAY intersectset

echo "Difference of testset and testset2 -> diffset:"
redis-cli BITS.DIFF diffset testset testset2
echo "Elements in diffset:"
redis-cli BITS.TOARRAY diffset

echo
echo "=== Information ==="
echo "Info about testset:"
redis-cli BITS.INFO testset

echo
echo "=== Remove Operations ==="
echo "Removing element 5 from testset:"
redis-cli BITS.REM testset 5
echo "Size after removal: $(redis-cli BITS.SIZE testset)"
echo "Elements after removal:"
redis-cli BITS.TOARRAY testset

echo
echo "=== Clear Operation ==="
echo "Clearing testset:"
redis-cli BITS.CLEAR testset
echo "Size after clear: $(redis-cli BITS.SIZE testset)"

echo
echo "=== Test Complete ==="
echo "Cleaning up test keys..."
redis-cli DEL testset testset2 unionset intersectset diffset

echo "Test completed successfully!"
