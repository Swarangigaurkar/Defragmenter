#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "disk_image.h"

#define OUTPUT_FILE_NAME "disk_defrag"

void readSuperblock(FILE *image, struct superblock *sb) {
    fseek(image, 512, SEEK_SET);
    fread(sb, sizeof(struct superblock), 1, image);
}

void writeSuperblock(FILE *image, struct superblock *sb) {
    fseek(image, 512, SEEK_SET);
    fwrite(sb, sizeof(struct superblock), 1, image);
}

void readInode(FILE *image, int inode_offset, int block_size, int numInodes, struct inode *nodes) {
    fseek(image, 1024 + (inode_offset * block_size), SEEK_SET);
    fread(nodes, sizeof(struct inode) * numInodes, 1, image);
}

void writeInode(FILE *image, int inode_offset, int block_size, int numInodes, struct inode *nodes) {
    fseek(image, 1024 + (inode_offset * block_size), SEEK_SET);
    fwrite(nodes, sizeof(struct inode) * numInodes, 1, image);
}

void readIndirectInodeBlock(FILE *image, int data_offset, int block_number, int block_size, int *d_block) {
    fseek(image, 1024 + (data_offset * block_size) + (block_number * block_size), SEEK_SET);
    fread(d_block, block_size, 1, image);
}

void writeIndirectInodeBlock(FILE *image, int data_offset, int block_number, int block_size, int *d_block) {
    fseek(image, 1024 + (data_offset * block_size) + (block_number * block_size), SEEK_SET);
    for (int i = 0; i < (int) (block_size / sizeof(int)); i++) {
        fwrite(&d_block[i], sizeof(int), 1, image);
    }
}

void readDataBlock(FILE *image, int data_offset, int block_number, int block_size, unsigned char* buffer) {
    fseek(image, 1024 + (data_offset * block_size) + (block_number * block_size), SEEK_SET);
    fread(buffer, block_size, 1, image);
}

void writeDataBlock(FILE *image, int data_offset, int block_number, int block_size, unsigned char* buffer) {
    fseek(image, 1024 + (data_offset * block_size) + (block_number * block_size), SEEK_SET);
    fwrite(buffer, block_size, 1, image);
}

int main(int argc, char *argv[]) {
    char *fileName = argv[1];

    FILE *image = fopen(fileName, "rb");

    if (image == NULL) {
        perror("Error opening file");
        return 1;
    }

    fseek(image, 0, SEEK_END);
    long size_of_file = ftell(image);
    fseek(image, 0, SEEK_SET);

    FILE* new_image = fopen(OUTPUT_FILE_NAME, "w+");
    fseek(new_image, 0L, SEEK_SET);

    struct superblock sb;
    readSuperblock(image, &sb);

    printf("Blocksize: %d\n", sb.blocksize);
    printf("Inode Offset: %d\n", sb.inode_offset);
    printf("Data Offset: %d\n", sb.data_offset);
    printf("Swap Offset: %d\n", sb.swap_offset);
    printf("Free Inode Head: %d\n", sb.free_inode);
    printf("Free Block Head: %d\n", sb.free_block);

    int numInodes = (int) ((sb.data_offset - sb.inode_offset) * sb.blocksize / sizeof(struct inode));

    struct inode* inodes;
    inodes = malloc(numInodes * sizeof(struct inode));
    readInode(image, sb.inode_offset, sb.blocksize, numInodes, inodes);

    int i = 0;

    struct inode_info *files;
    files = malloc(sizeof(struct inode_info) * numInodes);

    int inodeNum = 0;
    int inodeDatablockNum = 0;
    int inodeIndirectInodeBlockNum = 0;
    int inodeIndirectInodeDatablockNum = 0;

    int file_offset = 0;

    int niib = (int) (sb.blocksize / sizeof(int));
    int size_of_array = 10 + 4 * (1 + niib) + 1 + niib * (1 + niib) + 1 + niib * (1 + (niib * (1 + niib)));

    for (inodeNum = 0; inodeNum < numInodes; inodeNum++) {
        int new_data_block_num = 0;

        files[inodeNum].data_blocks = malloc(size_of_array * sizeof(int)); 

        for (inodeDatablockNum = 0; inodeDatablockNum < N_DBLOCKS; inodeDatablockNum++) {
            if (inodes[inodeNum].dblocks[inodeDatablockNum] == -1) {
                break;
            }

            files[inodeNum].data_blocks[new_data_block_num++] = inodes[inodeNum].dblocks[inodeDatablockNum];
            inodes[inodeNum].dblocks[inodeDatablockNum] = file_offset + new_data_block_num - 1;
        }

        for (inodeIndirectInodeBlockNum = 0; inodeIndirectInodeBlockNum < N_IBLOCKS; inodeIndirectInodeBlockNum++) {
            if (inodes[inodeNum].iblocks[inodeIndirectInodeBlockNum] == -1) {
                break;
            }

            files[inodeNum].data_blocks[new_data_block_num++] = -1;

            int *inode_indirect_inode_block_data_block_list;
            inode_indirect_inode_block_data_block_list = malloc(sb.blocksize);

            readIndirectInodeBlock(image, sb.data_offset, inodes[inodeNum].iblocks[inodeIndirectInodeBlockNum], sb.blocksize, inode_indirect_inode_block_data_block_list);
            inodes[inodeNum].iblocks[inodeIndirectInodeBlockNum] = file_offset + new_data_block_num - 1;

            for (inodeIndirectInodeDatablockNum = 0; inodeIndirectInodeDatablockNum < (int) (sb.blocksize / sizeof(int)); inodeIndirectInodeDatablockNum++) {
                if (inode_indirect_inode_block_data_block_list[inodeIndirectInodeDatablockNum] == -1) {
                    break;
                }

                files[inodeNum].data_blocks[new_data_block_num++] = inode_indirect_inode_block_data_block_list[inodeIndirectInodeDatablockNum];
                inode_indirect_inode_block_data_block_list[inodeIndirectInodeDatablockNum] = file_offset + new_data_block_num - 1;
            }

            writeIndirectInodeBlock(new_image, sb.data_offset, inodes[inodeNum].iblocks[inodeIndirectInodeBlockNum], sb.blocksize, inode_indirect_inode_block_data_block_list);
        }

        if (inodes[inodeNum].i2block != -1) {
            files[inodeNum].data_blocks[new_data_block_num++] = -1;

            int *inodeIndirect2InodeBlockDatablockList;
            inodeIndirect2InodeBlockDatablockList = malloc(sb.blocksize);

            readIndirectInodeBlock(image, sb.data_offset, inodes[inodeNum].i2block, sb.blocksize, inodeIndirect2InodeBlockDatablockList);
            inodes[inodeNum].i2block = file_offset + new_data_block_num - 1;

            for (int inode_indirect_2_inodeDatablockNum = 0; inode_indirect_2_inodeDatablockNum < (int) (sb.blocksize / sizeof(int)); inode_indirect_2_inodeDatablockNum++) {
                if (inodeIndirect2InodeBlockDatablockList[inode_indirect_2_inodeDatablockNum] == -1) {
                    break;
                }

                files[inodeNum].data_blocks[new_data_block_num++] = -1;

                int *inode_indirect_inode_block_data_block_list;
                inode_indirect_inode_block_data_block_list = malloc(sb.blocksize);

                readIndirectInodeBlock(image, sb.data_offset,inodeIndirect2InodeBlockDatablockList[inode_indirect_2_inodeDatablockNum],sb.blocksize, inode_indirect_inode_block_data_block_list);
                inodeIndirect2InodeBlockDatablockList[inode_indirect_2_inodeDatablockNum] = file_offset + new_data_block_num - 1;

                for (inodeIndirectInodeDatablockNum = 0; inodeIndirectInodeDatablockNum < (int) (sb.blocksize /sizeof(int)); inodeIndirectInodeDatablockNum++) {
                    if (inode_indirect_inode_block_data_block_list[inodeIndirectInodeDatablockNum] == -1) {
                        break;
                    }

                    files[inodeNum].data_blocks[new_data_block_num++] = inode_indirect_inode_block_data_block_list[inodeIndirectInodeDatablockNum];
                    inode_indirect_inode_block_data_block_list[inodeIndirectInodeDatablockNum] = file_offset + new_data_block_num - 1;
                }

                writeIndirectInodeBlock(new_image, sb.data_offset,inodeIndirect2InodeBlockDatablockList[inode_indirect_2_inodeDatablockNum],sb.blocksize, inode_indirect_inode_block_data_block_list);
            }

            writeIndirectInodeBlock(new_image, sb.data_offset, inodes[inodeNum].i2block, sb.blocksize, inodeIndirect2InodeBlockDatablockList);
        }

        if (inodes[inodeNum].i3block != -1) {
            files[inodeNum].data_blocks[new_data_block_num++] = -1;

            int *inodeIndirect3InodeBlockDatablockList;
            inodeIndirect3InodeBlockDatablockList = malloc(sb.blocksize);

            readIndirectInodeBlock(image, sb.data_offset, inodes[inodeNum].i3block, sb.blocksize, inodeIndirect3InodeBlockDatablockList);
            inodes[inodeNum].i3block = file_offset + new_data_block_num - 1;

            for (int inode_indirect_3_inodeDatablockNum = 0; inode_indirect_3_inodeDatablockNum < (int) (sb.blocksize / sizeof(int)); inode_indirect_3_inodeDatablockNum++) {
                if (inodeIndirect3InodeBlockDatablockList[inode_indirect_3_inodeDatablockNum] == -1) {
                    break;
                }

                files[inodeNum].data_blocks[new_data_block_num++] = -1;

                int *inodeIndirect2InodeBlockDatablockList;
                inodeIndirect2InodeBlockDatablockList = malloc(sb.blocksize);

                readIndirectInodeBlock(image, sb.data_offset, inodeIndirect3InodeBlockDatablockList[inode_indirect_3_inodeDatablockNum], sb.blocksize, inodeIndirect2InodeBlockDatablockList);
                inodeIndirect3InodeBlockDatablockList[inode_indirect_3_inodeDatablockNum] = file_offset + new_data_block_num - 1;

                for (int inode_indirect_2_inodeDatablockNum = 0; inode_indirect_2_inodeDatablockNum < (int) (sb.blocksize / sizeof(int)); inode_indirect_2_inodeDatablockNum++) {
                    if (inodeIndirect2InodeBlockDatablockList[inode_indirect_2_inodeDatablockNum] == -1) {
                        break;
                    }

                    files[inodeNum].data_blocks[new_data_block_num++] = -1;

                    int *inode_indirect_inode_block_data_block_list;
                    inode_indirect_inode_block_data_block_list = malloc(sb.blocksize);

                    readIndirectInodeBlock(image, sb.data_offset,inodeIndirect2InodeBlockDatablockList[inode_indirect_2_inodeDatablockNum],sb.blocksize, inode_indirect_inode_block_data_block_list);
                    inodeIndirect2InodeBlockDatablockList[inode_indirect_2_inodeDatablockNum] = file_offset + new_data_block_num - 1;

                    for (inodeIndirectInodeDatablockNum = 0; inodeIndirectInodeDatablockNum < (int) (sb.blocksize /sizeof(int)); inodeIndirectInodeDatablockNum++) {
                        if (inode_indirect_inode_block_data_block_list[inodeIndirectInodeDatablockNum] == -1) {
                            break;
                        }

                        files[inodeNum].data_blocks[new_data_block_num++] = inode_indirect_inode_block_data_block_list[inodeIndirectInodeDatablockNum];
                        inode_indirect_inode_block_data_block_list[inodeIndirectInodeDatablockNum] = file_offset + new_data_block_num - 1;
                    }

                    writeIndirectInodeBlock(new_image, sb.data_offset,inodeIndirect2InodeBlockDatablockList[inode_indirect_2_inodeDatablockNum],sb.blocksize, inode_indirect_inode_block_data_block_list);
                }

                writeIndirectInodeBlock(new_image, sb.data_offset, inodeIndirect3InodeBlockDatablockList[inode_indirect_3_inodeDatablockNum], sb.blocksize, inodeIndirect2InodeBlockDatablockList);
            }

            writeIndirectInodeBlock(new_image, sb.data_offset, inodes[inodeNum].i3block, sb.blocksize, inodeIndirect3InodeBlockDatablockList);
        }

        files[inodeNum].file_offset_data_block_num = file_offset;
        files[inodeNum].num_of_data_blocks = new_data_block_num;
        file_offset += files[inodeNum].num_of_data_blocks;
    }


    i = 0;

    while (i < numInodes) {
        for (int j=0; j< N_IBLOCKS; j++) {
            if (inodes[i].iblocks[j] == -1) {
                continue;
            }

            int *d_blocks;
            d_blocks = malloc(sb.blocksize);

            readIndirectInodeBlock(new_image, sb.data_offset, inodes[i].iblocks[j], sb.blocksize, d_blocks);

            for (int k = 0; k < (int) (sb.blocksize / sizeof(int)); k++) {
                if (d_blocks[k] == -1) {
                    break;
                }
            }
        }

        i++;
    }

    sb.free_block = file_offset;

    unsigned char* buffer;
    buffer = malloc(512);

    fseek(image, 0L, SEEK_SET);
    fread(buffer, 512, 1, image);

    fseek(new_image, 0L, SEEK_SET);
    fwrite(buffer, 512, 1, new_image);

    writeSuperblock(new_image, &sb);

    writeInode(new_image, sb.inode_offset, sb.blocksize, inodeNum, inodes);

    for (inodeNum = 0; inodeNum < numInodes; inodeNum++) {

        for (inodeDatablockNum = 0; inodeDatablockNum < files[inodeNum].num_of_data_blocks; inodeDatablockNum++) {
            printf("\r(%d/%d) ", inodeDatablockNum, files[inodeNum].num_of_data_blocks);

            if (files[inodeNum].data_blocks[inodeDatablockNum] == -1) {
                continue;
            }

            int old_data_block_num = files[inodeNum].data_blocks[inodeDatablockNum];
            int new_data_block_num = inodeDatablockNum + files[inodeNum].file_offset_data_block_num;

            unsigned char* buffer_;
            buffer_ = malloc(sb.blocksize);

            readDataBlock(image, sb.data_offset, old_data_block_num, sb.blocksize, buffer_);
            writeDataBlock(new_image, sb.data_offset, new_data_block_num, sb.blocksize, buffer_);
        }

        printf("\rDone\n");
    }

    int x = file_offset + 1;
    int y = -1;

    while (file_offset < sb.swap_offset - sb.data_offset - 1) {
        fwrite(&x, sizeof(int), 1, new_image);
        fseek(new_image, sb.blocksize - sizeof(int), SEEK_CUR);
        x++;
        file_offset++;
    }

    fwrite(&y, sizeof(int), 1, new_image);
    fseek(new_image, sb.blocksize - sizeof(int), SEEK_CUR);

    unsigned char* swap;
    swap = malloc(size_of_file - (1024 + (sb.data_offset * sb.blocksize) + ((sb.swap_offset - sb.data_offset) * sb.blocksize)));

    fseek(image, 1024 + (sb.data_offset * sb.blocksize) + ((sb.swap_offset - sb.data_offset) * sb.blocksize), SEEK_SET);
    fread(swap, size_of_file - (1024 + (sb.data_offset * sb.blocksize) + ((sb.swap_offset - sb.data_offset) * sb.blocksize)), 1, image);

    fseek(new_image, 1024 + (sb.data_offset * sb.blocksize) + ((sb.swap_offset - sb.data_offset) * sb.blocksize), SEEK_SET);
    fwrite(swap, size_of_file - (1024 + (sb.data_offset * sb.blocksize) + ((sb.swap_offset - sb.data_offset) * sb.blocksize)), 1, new_image);

    fclose(image);
    fclose(new_image);

    return 0;
}