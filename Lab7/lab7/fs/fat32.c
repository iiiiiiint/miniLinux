#include <fat32.h>
#include <printk.h>
#include <virtio.h>
#include <string.h>
#include <mbr.h>
#include <mm.h>

struct fat32_bpb fat32_header;

struct fat32_volume fat32_volume;

uint8_t fat32_buf[VIRTIO_BLK_SECTOR_SIZE];
uint8_t fat32_table_buf[VIRTIO_BLK_SECTOR_SIZE];

uint64_t cluster_to_sector(uint64_t cluster) {
    return (cluster - 2) * fat32_volume.sec_per_cluster + fat32_volume.first_data_sec;
}

uint32_t next_cluster(uint64_t cluster) {
    uint64_t fat_offset = cluster * 4;
    uint64_t fat_sector = fat32_volume.first_fat_sec + fat_offset / VIRTIO_BLK_SECTOR_SIZE;
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    int index_in_sector = fat_offset % (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
    return *(uint32_t*)(fat32_table_buf + index_in_sector);
}

void fat32_init(uint64_t lba, uint64_t size) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    uint64 reserve = fat32_header.rsvd_sec_cnt;
    uint64 Fat = fat32_header.num_fats * fat32_header.fat_sz32;
    fat32_volume.first_data_sec = lba + reserve+Fat;
    fat32_volume.sec_per_cluster = fat32_header.sec_per_clus;
    fat32_volume.first_fat_sec = lba + reserve;
    fat32_volume.fat_sz = fat32_header.fat_sz32;

    virtio_blk_read_sector(fat32_volume.first_data_sec, fat32_buf); // Get the root directory
    struct fat32_dir_entry *dir_entry = (struct fat32_dir_entry *)fat32_buf;
}

int is_fat32(uint64_t lba) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    if (fat32_header.boot_sector_signature != 0xaa55) {
        return 0;
    }
    return 1;
}

int next_slash(const char* path) {
    int i = 0;
    while (path[i] != '\0' && path[i] != '/') {
        i++;
    }
    if (path[i] == '\0') {
        return -1;
    }
    return i;
}

void to_upper_case(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] -= 32;
        }
    }
}

struct fat32_file fat32_open_file(const char *path) {
    // printk("in 32_file_open\n");
    char path_[strlen(path)];
    for(uint64 i = 0;i < strlen(path);i++){
        path_[i] = path[i+1];
    }
    uint64 temp = next_slash(path_);
    char t[strlen(path_)-temp-1];
    if(temp!=-1){
        for(uint64 i = 0;i<strlen(path_)-temp;i++){
            t[i] = path_[i+temp+1];
        }
    }
    struct fat32_file file;
    to_upper_case((char *)t);
    // printk("%d\n",strlen(t));
    virtio_blk_read_sector(fat32_volume.first_data_sec, fat32_buf); // Get the root directory
    struct fat32_dir_entry *dir_entry = (struct fat32_dir_entry *)fat32_buf;
    uint64 i = 0;
    for(i=0;i<512;i++){
        // printk("dir_entry[i].name=%s",dir_entry[i].name);
        if(memcmp(dir_entry[i].name, t, strlen(t))==0){
            // printk("find!%d\n", i);
            file.cluster = dir_entry[i].startlow | dir_entry[i].starthi << 16;
            file.dir.cluster = dir_entry[i].startlow | dir_entry[i].starthi << 16;
            uint64 temp = cluster_to_sector(file.dir.cluster);
            file.dir.index = i;
            break;
        }
    }
    // printk("leaving...\n");
    return file;
}

int64_t fat32_lseek(struct file* file, int64_t offset, uint64_t whence) {
    // printk("in lseek\n");
    if (whence == SEEK_SET) {
        file->cfo = offset;
    } else if (whence == SEEK_CUR) {
        file->cfo = file->cfo + offset;
    } else if (whence == SEEK_END) {
        /* Calculate file length */
        virtio_blk_read_sector(fat32_volume.first_data_sec, fat32_buf); // Get the root directory
        struct fat32_dir_entry *dir_entry = (struct fat32_dir_entry *)fat32_buf;
        file->cfo = dir_entry[file->fat32_file.dir.index].size;
    } else {
        printk("fat32_lseek: whence not implemented\n");
        while (1);
    }
    return file->cfo;
}

uint64_t fat32_table_sector_of_cluster(uint32_t cluster) {
    return fat32_volume.first_fat_sec + cluster / (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
}

int64_t fat32_extend_filesz(struct file* file, uint64_t new_size) {
    uint64_t sector = cluster_to_sector(file->fat32_file.dir.cluster) + file->fat32_file.dir.index / FAT32_ENTRY_PER_SECTOR;

    virtio_blk_read_sector(sector, fat32_table_buf);
    uint32_t index = file->fat32_file.dir.index % FAT32_ENTRY_PER_SECTOR;
    uint32_t original_file_len = ((struct fat32_dir_entry *)fat32_table_buf)[index].size;
    ((struct fat32_dir_entry *)fat32_table_buf)[index].size = new_size;

    virtio_blk_write_sector(sector, fat32_table_buf);

    uint32_t clusters_required = new_size / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint32_t clusters_original = original_file_len / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint32_t new_clusters = clusters_required - clusters_original;

    uint32_t cluster = file->fat32_file.cluster;
    while (1) {
        uint32_t next_cluster_number = next_cluster(cluster);
        if (next_cluster_number >= 0x0ffffff8) {
            break;
        }
        cluster = next_cluster_number;
    }

    for (int i = 0; i < new_clusters; i++) {
        uint32_t cluster_to_append;
        for (int j = 2; j < fat32_volume.fat_sz * VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t); j++) {
            if (next_cluster(j) == 0) {
                cluster_to_append = j;
                break;
            }
        }
        uint64_t fat_sector = fat32_table_sector_of_cluster(cluster);
        virtio_blk_read_sector(fat_sector, fat32_table_buf);
        uint32_t index_in_sector = cluster * 4 % VIRTIO_BLK_SECTOR_SIZE;
        *(uint32_t*)(fat32_table_buf + index_in_sector) = cluster_to_append;
        virtio_blk_write_sector(fat_sector, fat32_table_buf);
        cluster = cluster_to_append;
    }

    uint64_t fat_sector = fat32_table_sector_of_cluster(cluster);
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    uint32_t index_in_sector = cluster * 4 % VIRTIO_BLK_SECTOR_SIZE;
    *(uint32_t*)(fat32_table_buf + index_in_sector) = 0x0fffffff;
    virtio_blk_write_sector(fat_sector, fat32_table_buf);

    return 0;
}

int64_t fat32_read(struct file* file, void* buf, uint64_t len) {
    // printk("%d\n", file->cfo);
    virtio_blk_read_sector(fat32_volume.first_data_sec, fat32_buf); // Get the root directory
    struct fat32_dir_entry *dir_entry = (struct fat32_dir_entry *)fat32_buf;
    uint64 file_total_len = dir_entry[file->fat32_file.dir.index].size;
    // printk("total len = %d\n", file_total_len);
    uint64 cluster_size = fat32_volume.sec_per_cluster * 512;
    uint64 left_len = len;
    if(file_total_len - file->cfo < len){
        left_len = file_total_len - file->cfo;
    }
    uint64 ret = 0;
    uint32_t cluster = file->fat32_file.cluster + file->cfo/cluster_size;
    while(left_len > 0){
        uint64 sec = cluster_to_sector(cluster);
        virtio_blk_read_sector(sec, fat32_buf);
        uint64 offset = file->cfo % cluster_size;
        uint64 left_part_in_cluster = cluster_size - offset;
        // printk("offset = %d\n", offset);
        // printk("left_part = %d\n", left_part_in_cluster);
        // printk("left_len = %d\n", left_len);
        if(left_len >= left_part_in_cluster){
            memcpy(buf, fat32_buf + offset, left_part_in_cluster);
            file->cfo += left_part_in_cluster;
            left_len -= left_part_in_cluster;
            ret += left_part_in_cluster;
        }else{
            memcpy(buf, fat32_buf + offset, left_len);
            file->cfo += left_len;
            ret += left_len;
            left_len = 0;
        }
        uint32_t next_cluster_number = next_cluster(cluster);

        if (next_cluster_number >= 0x0ffffff8) {
            break;
        }
        cluster = next_cluster_number;
    }
    // printk("ret = %d", ret);
    return ret;
    /* todo: read content to buf, and return read length */
}

int64_t fat32_write(struct file* file, const void* buf, uint64_t len) {
    uint64 sec = cluster_to_sector(file->fat32_file.cluster);
    virtio_blk_read_sector(sec, fat32_buf);
    // printk("%d\n", file->cfo);
    memcpy(fat32_buf+file->cfo, buf, len);
    virtio_blk_write_sector(sec, fat32_buf);
    return 0;
    /* todo: fat32_write */
}