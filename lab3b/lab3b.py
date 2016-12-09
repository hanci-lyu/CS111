import csv
from collections import defaultdict

# outputfile
out = open("lab3b_check.txt", "w")

# error types
UNALLOCATED_BLOCK = 0
DUPLICATE_ALLOCATED_BLOCK = 1
UNALLOCATED_INODE = 2
MISSING_INODE = 3
INCORRECT_LINK_COUNT = 4
INCORRECT_DIRECTORY_ENTRY = 5
INVALID_BLOCK_POINTER = 7

def error(type, **args):
    if type == UNALLOCATED_BLOCK:
        out.write("UNALLOCATED BLOCK < {0} > REFERENCED BY".format(args['blockNum']) + "".join(" INODE < {0} > ENTRY < {1} >".format(inodeNum, entryNum) if indBlockNum == -1 else " INODE < {0} > INDIRECT BLOCK < {1} > ENTRY < {2} >".format(inodeNum, indBlockNum, entryNum) for inodeNum, indBlockNum, entryNum in args['refs']) + "\n")

    elif type == DUPLICATE_ALLOCATED_BLOCK:
        out.write("MULTIPLY REFERENCED BLOCK < {0} > BY".format(args['blockNum']) + "".join(" INODE < {0} > ENTRY < {1} >".format(inodeNum, entryNum) if indBlockNum == -1 else " INODE < {0} > INDIRECT BLOCK < {1} >) ENTRY < {2} >".format(inodeNum, indBlockNum, entryNum) for inodeNum, indBlockNum, entryNum in args['refs']) + "\n")

    elif type == UNALLOCATED_INODE:
        out.write("UNALLOCATED INODE < {0} > REFERENCED BY".format(args['inodeNum']) + "".join(" DIRECTORY < {0} > ENTRY < {1} >".format(dirInodeNum, entryNum) for dirInodeNum, entryNum in args['refs']) + "\n")
        
    elif type == MISSING_INODE:
        out.write("MISSING INODE < {0} > SHOULD BE IN FREE LIST < {1} >\n".format(args['inodeNum'], args['blockNum']))
        
    elif type == INCORRECT_LINK_COUNT:
        out.write("LINKCOUNT < {0} > IS < {1} > SHOULD BE < {2} >\n".format(args['inodeNum'], args['actualLinkCount'], args['expectedLinkCount']))
        
    elif type == INCORRECT_DIRECTORY_ENTRY:
        out.write("INCORRECT ENTRY IN < {0} > NAME < {1} > LINK TO < {2} > SHOULD BE < {3} >\n".format(args['dirInodeNum'], args['entryName'], args['actualInodeNum'], args['expectedInodeNum']))
        
    elif type == INVALID_BLOCK_POINTER:
        if args['indBlockNum'] == -1:
            out.write("INVALID BLOCK < {0} > IN INODE < {1} > ENTRY < {2} >\n".format(args['blockNum'], args['inodeNum'], args['entryNum']))
        else:
            out.write("INVALID BLOCK < {0} > IN INODE < {1} > INDIRECT BLOCK < {2} > ENTRY < {3} >\n".format(args['blockNum'], args['inodeNum'], args['indBlockNum'], args['entryNum']))
        
# group.csv
inodeBitmapBlocks = []
blockBitmapBlocks = []

# bitmap.csv
freeInodes = []
freeBlocks = []

# inode.csv
usedInodes = {}
usedBlocks = {}

# indirect.csv
indirectBlockEntries = defaultdict(dict)

# directory.csv
parentInode = {}
unallocatedInodes = defaultdict(list)

class Inode:
    def __init__(self, row):
        self.inodeNum = int(row[0])
        self.linkCount = int(row[5])
        self.blockCount = int(row[10])
        self.ptrs = map(lambda x: int(x, 16), row[-15:])
        self.refs = [] # element tuple (dirInodeNum, entryNum)

class Block:
    def __init__(self, blockNum):
        self.blockNum = blockNum
        self.refs = [] # element tuple (inodeNum, indBlockNum, entryNum)


# super.csv
with open('super.csv') as csvfile:
    reader = csv.reader(csvfile)
    row = reader.next()
    numInodes = int(row[1])
    numBlocks = int(row[2])
    blockSize = int(row[3])
    inodePerGroup = int(row[6])
    firstDataBlock = int(row[8])
    # print "numBlocks:", numBlocks
    # print "blockSize:", blockSize
    # print "firstDataBlock:", firstDataBlock
    

# group.csv
with open('group.csv') as csvfile:
    inodeBitmapBlocks, blockBitmapBlocks = zip(*[(int(row[4], 16), int(row[5], 16)) for row in csv.reader(csvfile)])
# print "inodeBitmapBlocks:", inodeBitmapBlocks
# print "blockBitmapBlocks:", blockBitmapBlocks

# bitmap.csv
with open('bitmap.csv') as csvfile:
    for row in csv.reader(csvfile):
        bitmapBlock, num = int(row[0], 16), int(row[1])
        if bitmapBlock in inodeBitmapBlocks:
            freeInodes.append(num)
        elif bitmapBlock in blockBitmapBlocks:
            freeBlocks.append(num)

# indirect
with open('indirect.csv') as csvfile:
    for row in csv.reader(csvfile):
        blockNum = int(row[0], 16)
        entryNum = int(row[1])
        ptr = int(row[2], 16)
        indirectBlockEntries[blockNum][entryNum] = ptr

# inode.csv
def checkPtr(ptr, inodeNum, indBlockNum, entryNum):
    if ptr == 0 or ptr >= numBlocks:
        error(INVALID_BLOCK_POINTER, blockNum=ptr, inodeNum=inodeNum, indBlockNum=indBlockNum, entryNum=entryNum)
        return False
    else:
        if ptr not in usedBlocks:
            usedBlocks[ptr] = Block(ptr)
        usedBlocks[ptr].refs.append((inodeNum, indBlockNum, entryNum))
        return True

def checkIndPtr(ptr, inodeNum, indBlockNum, entryNum, blocksLeft, depth):
    if blocksLeft <= 0 or depth < 0:
        return blocksLeft

    # cur ptr
    if checkPtr(ptr, inodeNum, indBlockNum, entryNum):
        blocksLeft -= 1

        # recursive ptr
        if (depth > 0):
            for e, p in indirectBlockEntries[ptr].items():
                blocksLeft = checkIndPtr(p, inodeNum, ptr, e, blocksLeft, depth - 1)
 
    return blocksLeft

with open('inode.csv') as csvfile:
    for row in csv.reader(csvfile):
        # record the inode
        inodeNum = int(row[0])
        inode = Inode(row)
        usedInodes[inodeNum] = inode
        blockCount = inode.blockCount

        # direct pointers
        for i in xrange(12):
            blockCount = checkIndPtr(inode.ptrs[i], inodeNum, -1, i, blockCount, 0)

        # indirect pointers
        blockCount = checkIndPtr(inode.ptrs[12], inodeNum, -1, i, blockCount, 1)

        # double indirect
        blockCount = checkIndPtr(inode.ptrs[13], inodeNum, -1, i, blockCount, 2)

        # triple indirect
        blockCount = checkIndPtr(inode.ptrs[14], inodeNum, -1, i, blockCount, 3)

# directory.csv
with open('directory.csv') as csvfile:
    parentFolderInodes = []
    for row in csv.reader(csvfile):
        dirInodeNum = int(row[0])
        entryNum = int(row[1])
        entryInodeNum = int(row[4])

        # record the parent link
        if entryNum != 0 and entryNum != 1 or entryInodeNum == dirInodeNum == 2:
            parentInode[entryInodeNum] = dirInodeNum

        # record the reference
        if entryInodeNum in usedInodes:
            usedInodes[entryInodeNum].refs.append((dirInodeNum, entryNum))
        else:
            unallocatedInodes[entryInodeNum].append((dirInodeNum, entryNum))

        # check directory entry "." and ".."
        if entryNum == 0 and entryInodeNum != dirInodeNum:
            error(INCORRECT_DIRECTORY_ENTRY, dirInodeNum=dirInodeNum, entryName='.', actualInodeNum=entryInodeNum, expectedInodeNum=dirInodeNum)

        # we cannot check ".." now, because we are not necessarily processing the inodes in topological order
        if entryNum == 1:
            parentFolderInodes.append((dirInodeNum, entryInodeNum))

# check ".." now
for dirInodeNum, entryInodeNum in parentFolderInodes:
    if entryInodeNum != parentInode[dirInodeNum]:
        error(INCORRECT_DIRECTORY_ENTRY, dirInodeNum=dirInodeNum, entryName='..', actualInodeNum=entryInodeNum, expectedInodeNum=parentInode[dirInodeNum])

# check unallocated inode
for entryInodeNum in unallocatedInodes:
    error(UNALLOCATED_INODE, inodeNum=entryInodeNum, refs=unallocatedInodes[entryInodeNum])


# check the allocated inode
for inodeNum, inode in usedInodes.items():
    linkCount = len(inode.refs)
    if inodeNum > 10 and linkCount == 0:
        error(MISSING_INODE, inodeNum=inodeNum, blockNum=inodeBitmapBlocks[(inodeNum-1)/inodePerGroup])
    elif linkCount != inode.linkCount:
        error(INCORRECT_LINK_COUNT, inodeNum=inodeNum, actualLinkCount=inode.linkCount, expectedLinkCount=linkCount)

# missing inode again
for inodeNum in xrange(11, numInodes+1):
    if inodeNum not in freeInodes and inodeNum not in usedInodes:
        error(MISSING_INODE, inodeNum=inodeNum, blockNum=inodeBitmapBlocks[(inodeNum-1)/inodePerGroup])

# dup allocated block
for blockNum in usedBlocks:
    if len(usedBlocks[blockNum].refs) > 1:
        error(DUPLICATE_ALLOCATED_BLOCK, blockNum=blockNum, refs=usedBlocks[blockNum].refs)

# unallocated block
for blockNum in set(freeBlocks) & set(usedBlocks.keys()):
    error(UNALLOCATED_BLOCK, blockNum=blockNum, refs=usedBlocks[blockNum].refs)
