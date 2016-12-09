#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define DIV_ROUNDUP(a, n) ((a + (n - 1)) / n)

/* superblock */
#define S_OFF_ICOUNT        0
#define S_OFF_BCOUNT        4
#define S_OFF_SUPER         20
#define S_OFF_BSHIFT        24
#define S_OFF_FSHIFT        28
#define S_OFF_BCOUNT_GRP    32
#define S_OFF_FCOUNT_GRP    36
#define S_OFF_ICOUNT_GRP    40
#define S_OFF_MAGIC         56
#define S_OFF_ISIZE         88

/* inode */
#define I_OFF_MODE      0
#define I_OFF_UID       2
#define I_OFF_SIZE      4
#define I_OFF_ATIME     8
#define I_OFF_CTIME     12
#define I_OFF_MTIME     16
#define I_OFF_GID       24
#define I_OFF_LCOUNT    26
#define I_OFF_SCOUNT    28
#define I_OFF_BLOCK     40
#define I_OFF_DIR_ACL   108
#define I_OFF_UID_HIGH  120

#define NDIR_BLOCKS 12
#define IND_BLOCK   NDIR_BLOCKS
#define DIND_BLOCK  (IND_BLOCK + 1)
#define TIND_BLOCK  (DIND_BLOCK + 1)
#define N_BLOCKS    (TIND_BLOCK + 1)

/* file type in inode */
#define FMASK   0xF000
#define IFLNK   0xA000
#define IFREG   0x8000
#define IFDIR   0x4000

/* directory entry */
#define D_MAX_NAME_LEN  255

struct super_block
{
    /* on disk structures*/
    uint32_t    s_inodes_count;
    uint32_t    s_blocks_count;
    uint32_t    s_log_block_size;
    uint32_t    s_log_frag_size;
    uint32_t    s_blocks_per_group;
    int32_t     s_frags_per_group;
    uint32_t    s_inodes_per_group;
    uint16_t    s_magic;
    uint32_t    s_first_data_block;
    uint16_t    s_inode_size;
    
    /* on memory helpers */
    uint32_t    s_block_size;
    uint32_t    s_frag_size;
};

struct block_group_desc
{
    uint32_t    bg_block_bitmap;
    uint32_t    bg_inode_bitmap;
    uint32_t    bg_inode_table;
    uint16_t    bg_free_blocks_count;
    uint16_t    bg_free_inodes_count;
    uint16_t    bg_used_dirs_count;
    uint16_t    bg_pad;
    uint32_t    bg_reserved[3];
};

struct block_group {
    /* on disk data */
    struct block_group_desc bg_desc;

    /* on memory */
    uint32_t bg_blocks_count;
    uint32_t bg_inodes_count;
};

struct inode 
{
    /* on disk data */
    uint16_t    i_mode;
    uint16_t    i_uid;
    uint16_t    i_gid;
    uint16_t    i_links_count;
    uint32_t    i_ctime;
    uint32_t    i_mtime;
    uint32_t    i_atime;
    uint32_t    i_size;
    uint32_t    i_sectors_count;
    uint32_t    i_block[N_BLOCKS];
    uint32_t    i_dir_acl;
    uint16_t    i_uid_high;

    /* aumgmented structures */
    uint32_t    i_inode;
    uint32_t    i_blocks_count;
    uint64_t    i_size64;
    struct inode *next;
};

struct dir_entry
{
    uint32_t    inode;
    uint16_t    rec_len;
    uint8_t     name_len;
    uint8_t     file_type;
    uint8_t     name[D_MAX_NAME_LEN+1];
};

void read_super_block(int fd, struct super_block *sb) 
{
    uint8_t buffer[1024];
    int num_read;

    // read the super block into buffer
    num_read = pread(fd, buffer, 1024, 1024);
    if (num_read == -1) {
        perror("read()");
        exit(1);
    } else if (num_read < 1024) {
        fprintf(stderr, "cannot read super block!\n");
        exit(1);
    }

    // fetch the fields
    memcpy(&sb->s_magic, buffer+S_OFF_MAGIC, 2);
    memcpy(&sb->s_inodes_count, buffer+S_OFF_ICOUNT, 4);
    memcpy(&sb->s_blocks_count, buffer+S_OFF_BCOUNT, 4);
    memcpy(&sb->s_log_block_size, buffer+S_OFF_BSHIFT, 4);
    memcpy(&sb->s_log_frag_size, buffer+S_OFF_FSHIFT, 4);
    memcpy(&sb->s_blocks_per_group, buffer+S_OFF_BCOUNT_GRP, 4);
    memcpy(&sb->s_frags_per_group, buffer+S_OFF_FCOUNT_GRP, 4);
    memcpy(&sb->s_inodes_per_group, buffer+S_OFF_ICOUNT_GRP, 4);
    memcpy(&sb->s_inode_size, buffer+S_OFF_ISIZE, 2);
    memcpy(&sb->s_first_data_block, buffer+S_OFF_SUPER, 4);
    
    // helpers
    sb->s_block_size = 1024 << sb->s_log_block_size;
    if (sb->s_log_frag_size >= 0)
        sb->s_frag_size = 1024 << sb->s_log_frag_size;
    else
        sb->s_frag_size = 1024 >> -sb->s_log_frag_size;

    // manually cal first data block
    if (sb->s_block_size <= 1024)
        sb->s_first_data_block = 1;
    else
        sb->s_first_data_block = 0;

#ifdef DEBUG
    printf("------------ Super block ------------\n");
    printf("Magic number: %" PRIx16 "\n", sb->s_magic);
    printf("Inode count: %" PRIu32 "\n", sb->s_inodes_count);
    printf("Block count: %" PRIu32 "\n", sb->s_blocks_count);
    printf("Block size: %" PRIu32 "\n", sb->s_block_size);
    printf("Fragment size: %" PRIu32 "\n", sb->s_frag_size);
    printf("Blocks per group: %" PRIu32 "\n", sb->s_blocks_per_group);
    printf("Inodes per group: %" PRIu32 "\n", sb->s_inodes_per_group);
    printf("Fragments per group: %" PRId32 "\n", sb->s_frags_per_group);
    printf("First data block: %" PRIu32 "\n", sb->s_first_data_block);
    printf("Inode size: %" PRIu16 "\n", sb->s_inode_size);
    printf("\n");
#endif
}

void read_block_groups(
    int fd,
    struct super_block *sb,
    struct block_group *bg,
    uint32_t block_group_count) 
{

#ifdef DEBUG
    assert(block_group_count == DIV_ROUNDUP(sb->s_inodes_count, sb->s_inodes_per_group));
#endif

    uint32_t offset = sb->s_block_size * (sb->s_first_data_block + 1);
    uint32_t blocks_count = sb->s_blocks_count; //- sb->s_first_data_block;
    uint32_t inodes_count = sb->s_inodes_count;
    int num_read, i;
    for (i = 0; i < block_group_count; i++) {
        num_read = pread(fd, &bg[i].bg_desc, sizeof(struct block_group_desc), offset);
        if (num_read == -1) {
            perror("read()");
            exit(1);
        } else if (num_read < sizeof(struct block_group_desc)) {
            fprintf(stderr, "cannot read block group descriptors!\n");
            exit(1);
        }

        bg[i].bg_blocks_count = sb->s_blocks_per_group < blocks_count ? sb->s_blocks_per_group : blocks_count;
        blocks_count -= bg[i].bg_blocks_count;
        bg[i].bg_inodes_count = sb->s_inodes_per_group < inodes_count ? sb->s_inodes_per_group : inodes_count;
        inodes_count -= bg[i].bg_inodes_count;
        offset += sizeof(struct block_group_desc);
    }

#ifdef DEBUG
    for (i = 0; i < block_group_count; i++) {
        printf("------------ Block group %d ------------\n", i);
        printf("Blocks: %" PRIu32 "\n", bg[i].bg_blocks_count);
        printf("Inodes: %" PRIu32 "\n", bg[i].bg_inodes_count);
        printf("Free blocks: %" PRIu16 "\n", bg[i].bg_desc.bg_free_blocks_count);
        printf("Free inodes: %" PRIu16 "\n", bg[i].bg_desc.bg_free_inodes_count);
        printf("Used directories: %" PRIu16 "\n", bg[i].bg_desc.bg_used_dirs_count);
        printf("Inode bitmap: %" PRIx32 "\n", bg[i].bg_desc.bg_inode_bitmap);
        printf("Block bitmap: %" PRIx32 "\n", bg[i].bg_desc.bg_block_bitmap);
        printf("Inode table: %" PRIx32 "\n", bg[i].bg_desc.bg_inode_table);
        printf("\n");
    }
#endif

}

void read_block(int fd, uint8_t *buf, uint32_t block_num, uint32_t block_size) {
    /* cache read */
    static int first = 1;
    static uint32_t cached_block_num = 0;
    static uint32_t cached_block_size = 0;
    static uint8_t *cached_buf = NULL;

    if (!first && cached_block_num == block_num && 
        cached_block_size == block_size &&
        cached_buf == buf)
        return;

    first = 0;
    cached_block_num = block_num;
    cached_block_size = block_size;
    cached_buf = buf;

    int num_read = pread(fd, buf, block_size, block_num * block_size);
    if (num_read == -1) {
        perror("read");
        exit(1);
    } else if (num_read < block_size) {
        fprintf(stderr, "cannot read block!\n");
        exit(1);
    }
}

struct inode *scan_bitmaps(
    int fd,
    struct super_block *sb,
    struct block_group *bg,
    uint32_t num_bg
    ) 
{   
    uint32_t free_inodes_count = 0;
    uint32_t free_blocks_count = 0;
    uint32_t i, j;
    FILE *csv = fopen("bitmap.csv", "w");

    // link list for the inode list
    struct inode *head = NULL;

    // allocate a block buffer
    uint8_t *bitmap = malloc(sb->s_block_size);
    uint8_t *buffer = malloc(sb->s_block_size);

    // scan bitmap
    for (i = 0u; i < num_bg; i++) {
        // inode bitmap
        read_block(fd, bitmap, bg[i].bg_desc.bg_inode_bitmap, sb->s_block_size);
        for (j = 0u; j < bg[i].bg_inodes_count; j++) {
            if ((bitmap[j / 8] & (1 << j % 8)) == 0) {
                /* free entry */
                free_inodes_count++;
                uint32_t inode = i * sb->s_inodes_per_group + j + 1;

                fprintf(csv, "%"PRIx32",", bg[i].bg_desc.bg_inode_bitmap);
                fprintf(csv, "%"PRIu32"\n", inode);

            } else {
                /* valid inode */
                struct inode *ino = malloc(sizeof(struct inode));
                ino->next = head;
                head = ino;

                // block containing the inode
                uint32_t containing_block = (j * sb->s_inode_size) / sb->s_block_size;
                read_block(fd, buffer, bg[i].bg_desc.bg_inode_table + containing_block, sb->s_block_size);

                // read inode
                uint8_t *base = buffer + (j * sb->s_inode_size) % sb->s_block_size;
                memcpy(&ino->i_mode, base + I_OFF_MODE, 2);
                memcpy(&ino->i_uid, base + I_OFF_UID, 2);
                memcpy(&ino->i_gid, base + I_OFF_GID, 2);
                memcpy(&ino->i_links_count, base + I_OFF_LCOUNT, 2);
                memcpy(&ino->i_ctime, base + I_OFF_CTIME, 4);
                memcpy(&ino->i_mtime, base + I_OFF_MTIME, 4);
                memcpy(&ino->i_atime, base + I_OFF_ATIME, 4);
                memcpy(&ino->i_size, base + I_OFF_SIZE, 4);
                memcpy(&ino->i_sectors_count, base + I_OFF_SCOUNT, 4);
                memcpy(&ino->i_block, base + I_OFF_BLOCK, 4 * N_BLOCKS);
                memcpy(&ino->i_dir_acl, base + I_OFF_DIR_ACL, 4);
                memcpy(&ino->i_uid_high, base + I_OFF_UID_HIGH, 2);

                ino->i_inode = i * sb->s_inodes_per_group + j + 1;  // start from 1
                ino->i_blocks_count = ino->i_sectors_count/(2<<sb->s_log_block_size);
                ino->i_size64 = ino->i_dir_acl;
                ino->i_size64 = (ino->i_size64 << 32) + ino->i_size;
            }
        }

        // block bitmap
        read_block(fd, bitmap, bg[i].bg_desc.bg_block_bitmap, sb->s_block_size);
        for (j = 0; j < bg[i].bg_blocks_count; j++) {
            if ((bitmap[j / 8] & (1 << j % 8)) == 0) {
                free_blocks_count++;
                uint32_t block = i * sb->s_blocks_per_group + j + sb->s_first_data_block;
                fprintf(csv, "%"PRIx32",", bg[i].bg_desc.bg_block_bitmap);
                fprintf(csv, "%"PRIu32"\n", block);
            }
        }
    }

    free(bitmap);
    free(buffer);
    fclose(csv);

#ifdef DEBUG
    printf("------------ free bitmap entries ------------\n");
    printf("free inodes (bitmap): %" PRIu32 "\n", free_inodes_count);
    printf("free blocks (bitmap): %" PRIu32 "\n", free_blocks_count);
#endif

    return head;
}

uint32_t scan_entry_one_block(uint8_t *buf, uint32_t block_size, 
                              FILE *csv, uint32_t parent_inode, 
                              uint32_t *entry_num) 
{
    uint32_t block_offset = 0;
    struct dir_entry entry;

    while (block_offset < block_size) {
        memcpy(&entry.inode, buf + block_offset, 4);
        memcpy(&entry.rec_len, buf + block_offset + 4, 2);

        if (entry.inode == 0)
            goto NEXT_ENTRY;

        memcpy(&entry.name_len, buf + block_offset + 6, 1);
        memcpy(&entry.file_type, buf + block_offset + 7, 1);
        memcpy(entry.name, buf + block_offset + 8, entry.name_len);
        entry.name[entry.name_len] = '\0';

        // output to csv
        fprintf(csv, "%"PRIu32",", parent_inode);
        fprintf(csv, "%"PRIu32",", *entry_num);
        fprintf(csv, "%"PRIu16",", entry.rec_len);
        fprintf(csv, "%"PRIu8",", entry.name_len);
        fprintf(csv, "%"PRIu32",", entry.inode);
        fprintf(csv, "\"%s\"", entry.name);
        fprintf(csv, "\n");

        // goto next entry
NEXT_ENTRY:
        block_offset += entry.rec_len;
        *entry_num += 1;
    }

#ifdef DEBUG
    assert(block_offset == block_size);
#endif
    return block_offset;
}

void _scan_directory_entry(uint32_t block,
                           uint32_t block_size,
                           int fd, FILE *csv, int depth, uint32_t parent_inode,
                           uint32_t *cur_offset, uint32_t directory_length, uint32_t *entry_num) 
{
    if (*cur_offset >= directory_length)
        return;

    // read in the block
    uint8_t *buf = malloc(block_size); 
    read_block(fd, buf, block, block_size);

    if (depth == 0) {
    // true data block
        *cur_offset += scan_entry_one_block(buf, block_size, csv, parent_inode, entry_num);

    } else {
    // indirect block
        uint32_t i, *blocks;
        blocks = (uint32_t *) buf;

        for (i = 0; i < block_size/4; i++) {
            if (*cur_offset >= directory_length)
                break;
            if (blocks[i] == 0)
                continue;
            _scan_directory_entry(blocks[i], block_size, fd, csv, depth-1, parent_inode, cur_offset, directory_length, entry_num);
        } 

    }

    free(buf);
}

void scan_directory_entry(int fd, struct super_block *sb, struct inode *ino) {
    FILE *csv = fopen("directory.csv", "w");

    for (; ino != NULL; ino = ino->next) 
    {
        // skip non directory inode
        if ((ino->i_mode & FMASK) != IFDIR)
            continue;

        // read each entry
        int i;
        uint32_t cur_offset = 0;
        uint32_t entry_num = 0;

        for (i = 0; i < NDIR_BLOCKS; i++)
            _scan_directory_entry(ino->i_block[i], sb->s_block_size, fd, csv, 0, ino->i_inode, &cur_offset, ino->i_size, &entry_num);
        _scan_directory_entry(ino->i_block[IND_BLOCK], sb->s_block_size, fd, csv, 1, ino->i_inode, &cur_offset, ino->i_size, &entry_num);
        _scan_directory_entry(ino->i_block[DIND_BLOCK], sb->s_block_size, fd, csv, 2, ino->i_inode, &cur_offset, ino->i_size, &entry_num);
        _scan_directory_entry(ino->i_block[TIND_BLOCK], sb->s_block_size, fd, csv, 3, ino->i_inode, &cur_offset, ino->i_size, &entry_num);
#ifdef DEBUG
        assert(cur_offset == ino->i_size);
#endif
    }

    fclose(csv);
}

void _scan_indirect_block_entry(uint32_t block, 
                                uint32_t block_size,
                                int fd, FILE *csv, 
                                int depth, int *blocks_left) 
{
    if (depth <= 0 || *blocks_left <= 0)
        return;

    // read in the indirect block
    uint8_t *buf = malloc(block_size); 
    read_block(fd, buf, block, block_size);
    *blocks_left -= 1; 

    uint32_t i, *blocks;
    blocks = (uint32_t *) buf;
    for (i = 0u; i < block_size/4; i++) 
    {
        // skip NULL pointers
        if (blocks[i] == 0)
            continue;

        fprintf(csv, "%"PRIx32",", block);
        fprintf(csv, "%"PRIu32",", i);
        fprintf(csv, "%"PRIx32"\n", blocks[i]);
        if (depth == 1)
            *blocks_left -= 1;

        _scan_indirect_block_entry(blocks[i], block_size, fd, csv, depth-1, blocks_left);
    }
    free(buf);
}

void scan_indirect_block_entry(int fd, struct super_block *sb, struct inode *ino) 
{
    FILE *csv = fopen("indirect.csv", "w");

    for (; ino != NULL; ino = ino->next) 
    {
        int blocks_left = ino->i_blocks_count - NDIR_BLOCKS;
        if (blocks_left > 0){
            _scan_indirect_block_entry(ino->i_block[IND_BLOCK], sb->s_block_size, fd, csv, 1, &blocks_left);
            _scan_indirect_block_entry(ino->i_block[DIND_BLOCK], sb->s_block_size, fd, csv, 2, &blocks_left);
            _scan_indirect_block_entry(ino->i_block[TIND_BLOCK], sb->s_block_size, fd, csv, 3, &blocks_left);
#ifdef DEBUG            
            assert(blocks_left <= 0);
#endif
        }
    }

    fclose(csv);
}

void write_csv_super_block(struct super_block *sb) {
    FILE *csv = fopen("super.csv", "w");

    fprintf(csv, "%"PRIx16",", sb->s_magic);
    fprintf(csv, "%"PRIu32",", sb->s_inodes_count);
    fprintf(csv, "%"PRIu32",", sb->s_blocks_count);
    fprintf(csv, "%"PRIu32",", sb->s_block_size);
    fprintf(csv, "%"PRIu32",", sb->s_frag_size);
    fprintf(csv, "%"PRIu32",", sb->s_blocks_per_group);
    fprintf(csv, "%"PRIu32",", sb->s_inodes_per_group);
    fprintf(csv, "%"PRId32",", sb->s_frags_per_group);
    fprintf(csv, "%"PRIu32"\n", sb->s_first_data_block);

    fclose(csv);
}

void write_csv_block_group(struct block_group *bg, uint32_t num_bg) {
    uint32_t i;
    FILE *csv = fopen("group.csv", "w");

    for (i = 0; i < num_bg; i++) {
        fprintf(csv, "%"PRIu32",", bg[i].bg_blocks_count);
        fprintf(csv, "%"PRIu16",", bg[i].bg_desc.bg_free_blocks_count);
        fprintf(csv, "%"PRIu16",", bg[i].bg_desc.bg_free_inodes_count);
        fprintf(csv, "%"PRIu16",", bg[i].bg_desc.bg_used_dirs_count);
        fprintf(csv, "%"PRIx32",", bg[i].bg_desc.bg_inode_bitmap);
        fprintf(csv, "%"PRIx32",", bg[i].bg_desc.bg_block_bitmap);
        fprintf(csv, "%"PRIx32"\n", bg[i].bg_desc.bg_inode_table);
    }
    fclose(csv);
}

void write_csv_inode_list(struct inode* ino) {
    char file_type;
    int i;
    uint32_t uid32;
    FILE *fh = fopen("inode.csv", "w");
    
    for (; ino != NULL; ino = ino->next) {
        switch (ino->i_mode & FMASK) {
        case IFLNK: file_type = 's'; break;
        case IFREG: file_type = 'f'; break;
        case IFDIR: file_type = 'd'; break;
        default:    file_type = '?'; break;
        }
        uid32 = ino->i_uid_high;
        uid32 = (uid32 << 16) | ino->i_uid;
        fprintf(fh, "%"PRIu32",", ino->i_inode);
        fprintf(fh, "%c,",        file_type);
        fprintf(fh, "%"PRIo16",", ino->i_mode);
        fprintf(fh, "%"PRIu32",", uid32);
        fprintf(fh, "%"PRIu16",", ino->i_gid);
        fprintf(fh, "%"PRIu16",", ino->i_links_count);
        fprintf(fh, "%"PRIx32",", ino->i_ctime);
        fprintf(fh, "%"PRIx32",", ino->i_mtime);
        fprintf(fh, "%"PRIx32",", ino->i_atime);
        // fprintf(fh, "%"PRIu32",", ino->i_size);
        fprintf(fh, "%"PRIu64",", ino->i_size64);
        fprintf(fh, "%"PRIu32, ino->i_blocks_count);
        for (i = 0; i < N_BLOCKS; i++)
            fprintf(fh, ",%"PRIx32, ino->i_block[i]);
        fprintf(fh, "\n");
    }


    fclose(fh);
}

int main(int argc, char **argv) 
{

    // open the disk file
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("cannot open disk image");
        exit(1);
    }

    // read super block
    struct super_block sb;
    read_super_block(fd, &sb);
    write_csv_super_block(&sb);

    // read block group descriptors
    struct block_group *bg;
    uint32_t num_bg;
    num_bg = DIV_ROUNDUP(sb.s_blocks_count, sb.s_blocks_per_group);
    bg = malloc(num_bg * sizeof(struct block_group));
    read_block_groups(fd, &sb, bg, num_bg);
    write_csv_block_group(bg, num_bg);

    // loop through the bitmaps
    struct inode *ino = scan_bitmaps(fd, &sb, bg, num_bg);

    // inodes
    write_csv_inode_list(ino);

    // directory entry
    scan_directory_entry(fd, &sb, ino);

    // indirect block entry
    scan_indirect_block_entry(fd, &sb, ino);


    return 0;
}
















