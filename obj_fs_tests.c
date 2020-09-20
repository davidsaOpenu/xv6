#include "string.h"  // xv6 version

//xv6 includes
#include "obj_disk.h"
#include "obj_cache.h"
#include "obj_log.h"
#include "fs.h"
#include "sleeplock.h"  // initsleeplock for inode tests

//tests includes
#include "obj_fs_tests_utilities.h"
#include "test.h"


#define TESTS_DEVICE 3


/**
 * Tests library variables
 */
int failed = 0;


/**
 * Disk layer tests
 */


const uint objects_table_bytes =
    OBJECTS_TABLE_SIZE * sizeof(ObjectsTableEntry);


/**
 * Tests that the metadata values inside the super block are intiialized
 * correctly by calling the get functions.
 */
TEST(initialization) {
    EXPECT_UINT_EQ(OBJECTS_TABLE_SIZE, max_objects());
    EXPECT_UINT_EQ(2, occupied_objects());
    EXPECT_UINT_EQ(STORAGE_DEVICE_SIZE, device_size());
    EXPECT_UINT_EQ(sizeof(SuperBlock) + objects_table_bytes,
                   occupied_bytes());
}


/**
 * Validates the correctness of getting the super block object.
 */
TEST(super_block_object) {
    uint size;
    ASSERT_NO_ERR(object_size(SUPER_BLOCK_ID, &size));
    ASSERT_UINT_EQ(sizeof(SuperBlock), size);

    SuperBlock sb;
    ASSERT_NO_ERR(get_object(SUPER_BLOCK_ID, &sb));

    EXPECT_UINT_EQ(STORAGE_DEVICE_SIZE, sb.storage_device_size);
    EXPECT_UINT_EQ(sizeof(SuperBlock), sb.objects_table_offset);
    EXPECT_UINT_EQ(OBJECTS_TABLE_SIZE, sb.objects_table_size);
    EXPECT_UINT_EQ(2, sb.occupied_objects);
    EXPECT_UINT_EQ(sizeof(SuperBlock) + objects_table_bytes,
                  sb.bytes_occupied);
}


/**
 * Validates the correctness of the objects table initial state by getting the
 * objects table object. We don't check the entries themsels because they are
 * checked by calling `get_object`. We only left the check that the rest of
 * the table is empty.
 */
TEST(table_object) {
    uint size;
    ASSERT_NO_ERR(object_size(OBJECT_TABLE_ID, &size));
    ASSERT_UINT_EQ(objects_table_bytes, size)

    ObjectsTableEntry table[OBJECTS_TABLE_SIZE];
    ASSERT_NO_ERR(get_object(OBJECT_TABLE_ID, &table));
    for (size_t i = 3; i < OBJECTS_TABLE_SIZE; ++i) {
        EXPECT_FALSE(table[i].occupied);
    }

    EXPECT_TRUE(table[0].occupied)
    EXPECT_UINT_EQ(sizeof(SuperBlock), table[0].size)
    EXPECT_TRUE(table[1].occupied)
    EXPECT_UINT_EQ(objects_table_bytes, table[1].size)
}


TEST(get_object_with_name_too_long) {
    uint size;
    EXPECT_UINT_EQ(OBJECT_NAME_TOO_LONG,
                   object_size("012345678901234567890123456789", &size));
}


TEST(get_non_existing_object) {
    uint size;
    EXPECT_UINT_EQ(OBJECT_NOT_EXISTS,
                   object_size("non_existing", &size));
}


TEST(add_single_object) {
    char my_string[] = "my super amazing string";
    ASSERT_NO_ERR(add_object(
        my_string, strlen(my_string) + 1, "simple_string"
    ));
    uint size;
    ASSERT_NO_ERR(object_size("simple_string", &size));
    ASSERT_UINT_EQ(strlen(my_string) + 1, size);
    char actual[strlen(my_string) + 1];
    ASSERT_NO_ERR(get_object("simple_string", actual));
    ASSERT_UINT_EQ(0, strcmp(actual, my_string));
}

TEST(add_object_already_exist) {
    uint placeholder;
    ASSERT_NO_ERR(
        add_object(&placeholder, sizeof(placeholder), "already_exist_0")
    );
    ASSERT_UINT_EQ(
        OBJECT_EXISTS,
        add_object(&placeholder, sizeof(placeholder), "already_exist_0")
    );
}


TEST(delete_existing_object) {
    uint placeholder;
    ASSERT_NO_ERR(
        add_object(&placeholder, sizeof(placeholder), "delete_0")
    );
    uint size;
    ASSERT_NO_ERR(object_size("delete_0", &size));
    ASSERT_NO_ERR(delete_object("delete_0"));
    ASSERT_UINT_EQ(OBJECT_NOT_EXISTS, object_size("delete_0", &size));
}


TEST(delete_no_existing_object) {
    ASSERT_UINT_EQ(OBJECT_NOT_EXISTS, delete_object("not_existing"));
}


TEST(rewrite_existing_object_with_shorter_data) {
    char first_string[] = "0123456789";
    char second_string[] = "abcdef";
    ASSERT_NO_ERR(add_object(
        first_string, strlen(first_string) + 1, "rewrite_shorter"
    ));

    //sanity check
    uint size;
    ASSERT_NO_ERR(object_size("rewrite_shorter", &size));
    ASSERT_UINT_EQ(strlen(first_string) + 1, size);

    //rewrite
    ASSERT_NO_ERR(rewrite_object(
        second_string, strlen(second_string) + 1, "rewrite_shorter"
    ));

    //validate the new size and data
    ASSERT_NO_ERR(object_size("rewrite_shorter", &size));
    ASSERT_UINT_EQ(strlen(second_string) + 1, size);
    char actual[strlen(second_string) + 1];
    ASSERT_NO_ERR(get_object("rewrite_shorter", actual));
    ASSERT_UINT_EQ(0, strcmp(actual, second_string));
}


TEST(rewrite_existing_object_with_longer_data) {
    char first_string[] = "0123456789";
    char second_string[] = "01234567890123456789";
    ASSERT_NO_ERR(add_object(
        first_string, strlen(first_string) + 1, "rewrite_longer"
    ));

    //sanity check
    uint size;
    ASSERT_NO_ERR(object_size("rewrite_longer", &size));
    ASSERT_UINT_EQ(strlen(first_string) + 1, size);

    //rewrite
    ASSERT_NO_ERR(rewrite_object(
        second_string, strlen(second_string) + 1, "rewrite_longer"
    ));

    //validate the new size and data
    ASSERT_NO_ERR(object_size("rewrite_longer", &size));
    ASSERT_UINT_EQ(strlen(second_string) + 1, size);
    char actual[strlen(second_string) + 1];
    ASSERT_NO_ERR(get_object("rewrite_longer", actual));
    ASSERT_UINT_EQ(0, strcmp(actual, second_string));
}


TEST(writing_multiple_objects) {
    const char* objects_data[3] = {
        "first data",
        "second data",
        "third data"
    };
    const char* objects_name[3] = {
        "writing multiple 1",
        "writing multiple 2",
        "writing multiple 3",
    };
    for (uint i = 0; i < 3; ++i) {
        ASSERT_NO_ERR(add_object(
            objects_data[i], strlen(objects_data[i]) + 1, objects_name[i]
        ));
    }
    for (uint i = 0; i < 3; ++i) {
        uint size;
        ASSERT_NO_ERR(object_size(objects_name[i], &size));
        ASSERT_UINT_EQ(strlen(objects_data[i]) + 1, size);
        char actual_data[size];
        ASSERT_NO_ERR(get_object(objects_name[i], actual_data));
        ASSERT_TRUE(strcmp(objects_data[i], actual_data) == 0);
    }
}

TEST(add_to_full_table) {
    ObjectsTableEntry original[OBJECTS_TABLE_SIZE];
    ObjectsTableEntry table[OBJECTS_TABLE_SIZE];
    ASSERT_NO_ERR(get_object(OBJECT_TABLE_ID, &table));
    memmove(original, table, sizeof(table));
    for (uint i = 0; i < OBJECTS_TABLE_SIZE; ++i) {
        table[i].occupied = 1;
    }
    ASSERT_NO_ERR(rewrite_object(table, sizeof(table), OBJECT_TABLE_ID));

    uint data;
    ASSERT_UINT_EQ(
        OBJECTS_TABLE_FULL,
        add_object(&data, sizeof(data), "non existing object")
    );
    ASSERT_NO_ERR(rewrite_object(original, sizeof(original), OBJECT_TABLE_ID));
}


uint find_object_offset(const char* object_name) {
    ObjectsTableEntry table[OBJECTS_TABLE_SIZE];
    get_object(OBJECT_TABLE_ID, &table);
    uint address = -1;
    for (uint i = 0; i < OBJECTS_TABLE_SIZE; ++i) {
        if (strcmp(table[i].object_id, object_name) == 0) {
            address = table[i].disk_offset;
            break;
        }
    }
    return address;
}


TEST(reusing_freed_space) {
    uint data;
    ASSERT_NO_ERR(add_object(&data, sizeof(data), "reusing object 1"));
    uint obj_1_offset = find_object_offset("reusing object 1");
    ASSERT_TRUE(obj_1_offset != -1);

    ASSERT_NO_ERR(delete_object("reusing object 1"));
    ASSERT_NO_ERR(add_object(&data, sizeof(data), "reusing object 2"));
    uint obj_2_offset = find_object_offset("reusing object 2");
    ASSERT_UINT_EQ(obj_1_offset, obj_2_offset);
}


TEST(add_when_there_is_no_more_disk_left) {
    ASSERT_UINT_EQ(
        NO_DISK_SPACE_FOUND,
        add_object(NULL, STORAGE_DEVICE_SIZE + 1, "object too large")
    );
}


/**
 * The following tests validate the correctness of the cache layer.
 * The tests use the `objects_cache_hits` and `objects_cache_misses` methods
 * to check the cache behavior vs the expected flow.
 */


TEST(get_object_in_cache) {
    char my_string[] = "my super amazing string";
    const char* obj_name = "get_object_in_cache";
    // inserting the object through the cache keeps it inside it
    cache_add_object(my_string, strlen(my_string) + 1, obj_name);

    uint misses_at_start = objects_cache_misses();
    uint hits_at_start = objects_cache_hits();

    // validate correctness
    char actual[strlen(my_string) + 1];
    ASSERT_NO_ERR(cache_get_object(name, actual));
    ASSERT_UINT_EQ(0, strcmp(actual, my_string));

    // valiadte hits and misses
    EXPECT_UINT_EQ(0, objects_cache_misses() - misses_at_start);
    EXPECT_UINT_EQ(1, objects_cache_hits() - hits_at_start);
}


TEST(get_object_not_in_cache) {
    char my_string[] = "my super amazing string";
    const char* obj_name = "object_no_cache_00";
    // inserting the object WITHOUT going through the cache
    add_object(my_string, strlen(my_string) + 1, obj_name);

    uint misses_at_start = objects_cache_misses();
    uint hits_at_start = objects_cache_hits();

    // validate correctness
    char actual[strlen(my_string) + 1];
    ASSERT_NO_ERR(cache_get_object(obj_name, actual));
    ASSERT_UINT_EQ(0, strcmp(actual, my_string));

    // valiadte hits and misses
    EXPECT_UINT_EQ(1, objects_cache_misses() - misses_at_start);
    ASSERT_UINT_EQ(0, objects_cache_hits() - hits_at_start);

    // re-accessing the object now set "hit" because it's in the cache from
    // previous attempt.
    ASSERT_NO_ERR(cache_get_object(obj_name, actual));
    EXPECT_UINT_EQ(1, objects_cache_misses() - misses_at_start);
    ASSERT_UINT_EQ(1, objects_cache_hits() - hits_at_start);
}


TEST(get_object_size_in_cache) {
    char my_string[] = "my super amazing string";
    const char* obj_name = "object_in_cache_01";
    // inserting the object WITH going through the cache
    cache_add_object(my_string, strlen(my_string) + 1, obj_name);

    uint misses_at_start = objects_cache_misses();
    uint hits_at_start = objects_cache_hits();

    ASSERT_NO_ERR(cache_get_object(obj_name, my_string));

    // valiadte hits and misses
    EXPECT_UINT_EQ(0, objects_cache_misses() - misses_at_start);
    ASSERT_UINT_EQ(1, objects_cache_hits() - hits_at_start);
}


TEST(get_object_size_not_in_cache_and_doesnt_add_to_cache) {
    char my_string[] = "my super amazing string";
    const char* obj_name = "object_no_cache_02";
    // inserting the object WITHOUT going through the cache
    add_object(my_string, strlen(my_string) + 1, obj_name);

    uint misses_at_start = objects_cache_misses();
    uint hits_at_start = objects_cache_hits();

    // validate correctness
    uint size;
    ASSERT_NO_ERR(cache_object_size(obj_name, &size));
    ASSERT_UINT_EQ(strlen(my_string) + 1, size);

    // valiadte hits and misses
    EXPECT_UINT_EQ(1, objects_cache_misses() - misses_at_start);
    ASSERT_UINT_EQ(0, objects_cache_hits() - hits_at_start);

    ASSERT_NO_ERR(cache_object_size(obj_name, &size));
    EXPECT_UINT_EQ(2, objects_cache_misses() - misses_at_start);
    ASSERT_UINT_EQ(0, objects_cache_hits() - hits_at_start);
}


TEST(object_too_large_not_inserted_to_cache) {
    char large_data[cache_max_object_size() * 2];
    const char* obj_name = "object_no_cache_03";

    for (uint i = 0; i < sizeof(large_data); ++i) {
        large_data[i] = i % 256;
    }

    uint misses_at_start = objects_cache_misses();
    uint hits_at_start = objects_cache_hits();

    cache_add_object(large_data, sizeof(large_data), obj_name);

    // validate correctness
    char actual[sizeof(large_data)];
    ASSERT_NO_ERR(cache_get_object(obj_name, actual));
    for (uint i = 0; i < sizeof(large_data); ++i) {
        ASSERT_UINT_EQ(large_data[i], actual[i]);
    }

    // valiadte hits and misses
    EXPECT_UINT_EQ(1, objects_cache_misses() - misses_at_start);
    ASSERT_UINT_EQ(0, objects_cache_hits() - hits_at_start);
}


/**
 * Logbook layer tests
 */


TEST(logbook_add_object_regular_flow) {
    const char* obj_name = "log_add_test_obj";
    char my_string[] = "my super amazing string";
    ASSERT_NO_ERR(log_add_object(
        my_string, strlen(my_string) + 1, obj_name
    ));
    uint size;
    ASSERT_NO_ERR(cache_object_size(obj_name, &size));
    ASSERT_UINT_EQ(strlen(my_string) + 1, size);
    char actual[strlen(my_string) + 1];
    ASSERT_NO_ERR(cache_get_object(obj_name, actual));
    ASSERT_UINT_EQ(0, strcmp(actual, my_string));
}


TEST(logbook_rewrite_object_regular_flow) {
    const char* obj_name = "log_rw_test_obj";
    char first_string[] = "0123456789";
    char second_string[] = "abcdef";
    ASSERT_NO_ERR(log_add_object(
        first_string, strlen(first_string) + 1, obj_name
    ));

    uint before, after;
    get_objects_table_index(obj_name, &before);
    //rewrite
    ASSERT_NO_ERR(log_rewrite_object(
        second_string, strlen(second_string) + 1, obj_name
    ));
    get_objects_table_index(obj_name, &after);

    //validate the new size and data
    uint size;
    ASSERT_NO_ERR(cache_object_size(obj_name, &size));
    ASSERT_UINT_EQ(strlen(second_string) + 1, size);
    char actual[strlen(second_string) + 1];
    ASSERT_NO_ERR(cache_get_object(obj_name, actual));
}


TEST(logbook_delete_object_regular_flow) {
    const char* object_name = "log delete obj";
    uint placeholder;
    ASSERT_NO_ERR(
        log_add_object(&placeholder, sizeof(placeholder), object_name)
    );
    ASSERT_NO_ERR(log_delete_object(object_name));
    uint size;
    ASSERT_UINT_EQ(OBJECT_NOT_EXISTS, cache_object_size(object_name, &size));
}


/**
 * Inodes layer tests
 */


/// Tests only that the function doesn't fail in segfault/deadlock
TEST(initialize_icache) {
    iinit(TESTS_DEVICE);
}


TEST(correct_inode_name) {
    uint inode_number = 123456;
    const char* expected = "inode\x8c\xd3\x87\x80\x80";
    char actual[INODE_NAME_LENGTH];
    inode_name(actual, inode_number);
    ASSERT_UINT_EQ(0, strcmp(expected, actual));
}


/// checking the `type` correctness is done by `ilock` in next tests.
TEST(ialloc_data_correctness) {
    struct inode* id = ialloc(0x12, 0x34);
    ASSERT_UINT_EQ(0x12, id->dev);
    ASSERT_UINT_EQ(0, id->valid);  // not loaded from the disk
    ASSERT_UINT_EQ(1, id->ref);
    ASSERT_UINT_EQ(0, id->data_object_name[0]);
    ilock(id);
}


TEST(ialloc_advancing_inodes_counter) {
    struct inode* id1 = ialloc(0, 0);
    struct inode* id2 = ialloc(0, 0);
    ASSERT_UINT_EQ(id1->inum + 1, id2->inum);
}


TEST(ialloc_store_in_disk) {
    struct inode* id = ialloc(0x12, 0x34);
    char iname[INODE_NAME_LENGTH];
    inode_name(iname, id->inum);
    struct inode loaded;
    ASSERT_NO_ERR(cache_get_object(iname, &loaded));
    iput(id);
}


TEST(iget_from_cache) {
    uint inum = 123;
    struct inode* id = iget(12, inum);
    ASSERT_UINT_EQ(12, id->dev);
    ASSERT_UINT_EQ(inum, id->inum);
    ASSERT_UINT_EQ(0, id->valid);
    ASSERT_UINT_EQ(1, id->ref);
    iput(id);
}


TEST(iget_add_new_inode_to_cache) {
    uint inum = 124;
    struct inode* id1 = iget(0, inum);
    struct inode* id2 = iget(0, inum);
    ASSERT_UINT_EQ(inum, id1->inum);
    ASSERT_UINT_EQ(inum, id2->inum);
    ASSERT_UINT_EQ(2, id1->ref);
    ASSERT_UINT_EQ(2, id2->ref);
    ASSERT_UINT_EQ(0, id1->valid);
    ASSERT_UINT_EQ(0, id2->valid);
    iput(id1);
    iput(id2);
}


TEST(iupdate_correctness) {
    struct inode* id = ialloc(0x12, 0);
    //`iupdate` save the inode to the disk, hence we check the `dinode` object.
    struct dinode read_inode;
    char iname[INODE_NAME_LENGTH];
    inode_name(iname, id->inum);

    id->type = 0x13;
    memmove(
        id->data_object_name,
        "some object",
        strlen("some object") + 1
    );
    iupdate(id);
    ASSERT_NO_ERR(cache_get_object(iname, &read_inode));
    ASSERT_UINT_EQ(0x13, read_inode.type);
    ASSERT_UINT_EQ(0, strcmp(read_inode.data_object_name, "some object"));

    id->type = 0x14;
    iupdate(id);
    ASSERT_NO_ERR(cache_get_object(iname, &read_inode));
    ASSERT_UINT_EQ(0x14, read_inode.type);
    iput(id);
}


TEST(idup_correctness) {
    uint inum = 125;
    struct inode* id1 = iget(0, inum);
    struct inode* id2 = idup(id1);
    ASSERT_UINT_EQ(inum, id1->inum);
    ASSERT_UINT_EQ(inum, id2->inum);
    ASSERT_UINT_EQ(2, id1->ref);
    ASSERT_UINT_EQ(2, id2->ref);
    ASSERT_UINT_EQ(0, id1->valid);
    ASSERT_UINT_EQ(0, id2->valid);
    iput(id1);
    iput(id2);
}


TEST(ilock_read_from_disk) {
    struct inode* id = ialloc(0x12, 0x34);
    ASSERT_UINT_EQ(0, id->valid);
    ilock(id);
    ASSERT_UINT_EQ(1, id->valid);
    ASSERT_UINT_EQ(0x34, id->type);
    iunlockput(id);
}


int ilock_read_from_disk_invalid_inum_panic = 0;
void ilock_read_from_disk_invalid_inum_panic_handler(void) {
    ilock_read_from_disk_invalid_inum_panic = 1;
}
/**
 * In regular flow, this would cause `panic`. In the tests, we validate that
 * this edge case is covered by verifing that the flow stopped in `return`.
 */
TEST(ilock_read_from_disk_invalid_inum) {
    set_panic_handler(ilock_read_from_disk_invalid_inum_panic_handler);
    struct inode id;
    id.valid = 0;
    id.inum = (uint)-1;
    id.ref = 1;
    initsleeplock(&id.lock, "inode");
    ilock(&id);
    set_panic_handler(default_panic_handler);
    ASSERT_UINT_EQ(0, id.valid);
    iunlock(&id);
    ASSERT_UINT_EQ(1, ilock_read_from_disk_invalid_inum_panic);
}


TEST(iput_no_links) {
    const char* test_object = "idelete_obj1";
    const char* data = "some data for the test";
    log_add_object(data, strlen(data) + 1, test_object);
    struct inode* id = ialloc(0x12, 0x34);
    //use ilock to make the inode valid
    ilock(id);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    char iname[INODE_NAME_LENGTH];
    inode_name(iname, id->inum);
    uint size;
    ASSERT_NO_ERR(cache_object_size(test_object, &size));
    ASSERT_NO_ERR(cache_object_size(iname, &size));
    //set the inode data object
    iupdate(id);
    iunlockput(id);
    //validate the data object is deleted
    ASSERT_UINT_EQ(OBJECT_NOT_EXISTS, cache_object_size(test_object, &size));
    //validate the inode object is deleted
    ASSERT_UINT_EQ(OBJECT_NOT_EXISTS, cache_object_size(iname, &size));
}


TEST(iput_with_links) {
    const char* test_object = "idelete_obj1";
    const char* data = "some data for the test";
    log_add_object(data, strlen(data) + 1, test_object);
    struct inode* id = ialloc(0x12, 0x34);
    //use ilock to make the inode valid
    ilock(id);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    id->nlink = 1;
    char iname[INODE_NAME_LENGTH];
    inode_name(iname, id->inum);
    uint size;
    //set the inode data object
    iupdate(id);
    iunlockput(id);
    ASSERT_NO_ERR(cache_object_size(test_object, &size));
    ASSERT_NO_ERR(cache_object_size(iname, &size));
    ASSERT_NO_ERR(log_delete_object(test_object));
    ASSERT_NO_ERR(log_delete_object(iname));
}


TEST(stati_correctness_data_object_no_exists) {
    struct stat stat;
    struct inode* id = ialloc(0, 0);
    id->type = 0x17;
    stati(id, &stat);
    ASSERT_UINT_EQ(0x17, stat.type);
    ASSERT_UINT_EQ(id->inum, stat.ino);
    ASSERT_UINT_EQ(0, stat.size);
    iput(id);
}


TEST(stati_correctness_data_object_exists) {
    struct stat stat;
    struct inode* id = ialloc(0, 0);
    const char* test_object = "stati_object1";
    const char* data = "some data for the test";
    log_add_object(data, strlen(data) + 1, test_object);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    stati(id, &stat);
    ASSERT_UINT_EQ(strlen(data) + 1, stat.size);
    iput(id);
}


TEST(readi_from_existing_without_offset) {
    struct inode* id = ialloc(0, 0);
    const char* test_object = "readi_obj_1";
    const char* data = "abcdefghijklmnopqrstuvwxyz";
    log_add_object(data, strlen(data) + 1, test_object);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    char actual[10];
    readi(id, actual, 0, 10);
    ASSERT_UINT_EQ(10, readi(id, actual, 0, 10));
    iput(id);
}


TEST(readi_from_existing_with_offset) {
    struct inode* id = ialloc(0, 0);
    const char* test_object = "readi_obj_2";
    const char* data = "abcdefghijklmnopqrstuvwxyz";
    log_add_object(data, strlen(data), test_object);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    char actual[10];
    ASSERT_UINT_EQ(10, readi(id, actual, 5, 10));
    ASSERT_UINT_EQ(0, strncmp(data + 5, actual, 10));
    iput(id);
}


int readi_from_non_existing_panic = 0;
void readi_from_non_existing_panic_handler(void) {
    readi_from_non_existing_panic = 1;
}
TEST(readi_from_non_existing) {
    set_panic_handler(readi_from_non_existing_panic_handler);
    struct inode* id = ialloc(0, 0);
    char actual[10];
    readi(id, actual, 0, 10);
    iput(id);
    set_panic_handler(default_panic_handler);
    ASSERT_UINT_EQ(1, readi_from_non_existing_panic);
}


TEST(readi_read_too_much) {
    struct inode* id = ialloc(0, 0);
    const char* test_object = "readi_obj_2";
    const char* data = "abcdefghijklmnopqrstuvwxyz";
    log_add_object(data, strlen(data), test_object);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    char actual[strlen(data)];
    ASSERT_UINT_EQ(strlen(data), readi(id, actual, 0, 100));
    ASSERT_UINT_EQ(0, strncmp(data, actual, strlen(data)));
    iput(id);
}


TEST(readi_read_offset_too_large) {
    struct inode* id = ialloc(0, 0);
    const char* test_object = "readi_obj_2";
    const char* data = "abcdefghijklmnopqrstuvwxyz";
    log_add_object(data, strlen(data), test_object);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    char actual[strlen(data)];
    ASSERT_UINT_EQ(-1, readi(id, actual, 100, 100));
    iput(id);
}

int writei_from_non_existing_panic = 0;
void writei_from_non_existing_panic_handler(void) {
    writei_from_non_existing_panic = 1;
}
TEST(writei_from_non_existing) {
    set_panic_handler(writei_from_non_existing_panic_handler);
    struct inode* id = ialloc(0, T_FILE);
    const char* write_data = "abcdefghijklmnopqrstuvwxyz";
    writei(id, write_data, 0, strlen(write_data));
    set_panic_handler(default_panic_handler);
    iput(id);
}


int compare_object_data_strings(const char* object_name, const char* expected) {
    uint size;
    if (cache_object_size(object_name, &size) != NO_ERR) {
        panic("failed getting object size");
    }
    char obj_data[size];
    if (cache_get_object(object_name, obj_data) != NO_ERR) {
        panic("failed getting object data§");
    }
    return strcmp(obj_data, expected);
}


TEST(writei_from_existing_without_offset) {
    struct inode* id = ialloc(0, T_FILE);
    ilock(id);
    const char* test_object = "write_obj_2";
    const char* original_data = "abcdefghijklmnopqrstuvwxyz";
    log_add_object(original_data, strlen(original_data) + 1, test_object);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    iupdate(id);
    ASSERT_UINT_EQ(0, compare_object_data_strings(test_object, original_data));
    ASSERT_UINT_EQ(4, writei(id, "1234", 0, 4));
    ASSERT_UINT_EQ(0, compare_object_data_strings(test_object, "1234efghijklmnopqrstuvwxyz"));
    iunlockput(id);
}


TEST(writei_from_existing_with_offset) {
    struct inode* id = ialloc(0, T_FILE);
    ilock(id);
    const char* test_object = "write_obj_3";
    const char* original_data = "abcdefghijklmnopqrstuvwxyz";
    log_add_object(original_data, strlen(original_data) + 1, test_object);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    iupdate(id);
    ASSERT_UINT_EQ(0, compare_object_data_strings(test_object, original_data));
    ASSERT_UINT_EQ(4, writei(id, "1234", 2, 4));
    ASSERT_UINT_EQ(0, compare_object_data_strings(test_object, "ab1234ghijklmnopqrstuvwxyz"));
    iunlockput(id);
}


TEST(writei_write_beyond_fail) {
    struct inode* id = ialloc(0, T_FILE);
    ilock(id);
    const char* test_object = "write_obj_4";
    const char* original_data = "abcdef";
    log_add_object(original_data, strlen(original_data) + 1, test_object);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    iupdate(id);
    ASSERT_UINT_EQ(0, compare_object_data_strings(test_object, original_data));
    ASSERT_UINT_EQ(10, writei(id, "123456789", 3, 10));
    ASSERT_UINT_EQ(0, compare_object_data_strings(test_object, "abc123456789"));
    iunlockput(id);
}


TEST(writei_write_append) {
    struct inode* id = ialloc(0, T_FILE);
    ilock(id);
    const char* test_object = "write_obj_5";
    const char* original_data = "abc";
    log_add_object(original_data, strlen(original_data), test_object);
    memmove(
        id->data_object_name,
        test_object,
        strlen(test_object) + 1
    );
    iupdate(id);
    ASSERT_UINT_EQ(3, writei(id, "123", strlen(original_data), 3));
    char actual[6];
    readi(id, actual, 0, 6);
    ASSERT_UINT_EQ(0, strncmp("abc123", actual, 6));
    iunlockput(id);
}


int main() {
    printf("[===========]\n");
    init_obj_fs();
    init_objects_cache();
    init_objfs_log();

    // Driver layer
    run_test(initialization);
    run_test(super_block_object);
    run_test(table_object);
    run_test(add_single_object);
    run_test(add_object_already_exist);
    run_test(delete_existing_object);
    run_test(rewrite_existing_object_with_shorter_data);
    run_test(rewrite_existing_object_with_longer_data);
    run_test(writing_multiple_objects);
    run_test(get_object_with_name_too_long);
    run_test(get_non_existing_object);
    run_test(add_to_full_table);
    run_test(reusing_freed_space);
    run_test(add_when_there_is_no_more_disk_left);

    // Cache layer
    run_test(get_object_in_cache);
    run_test(get_object_not_in_cache);
    run_test(get_object_size_in_cache);
    run_test(get_object_size_not_in_cache_and_doesnt_add_to_cache);
    run_test(object_too_large_not_inserted_to_cache);

    // Logbook layer
    run_test(logbook_add_object_regular_flow);
    run_test(logbook_rewrite_object_regular_flow);
    run_test(logbook_delete_object_regular_flow);

    // Inodes layer
    run_test(initialize_icache);
    run_test(correct_inode_name);
    run_test(ialloc_data_correctness);
    run_test(ialloc_advancing_inodes_counter);
    run_test(ialloc_store_in_disk);
    run_test(iget_from_cache);
    run_test(iget_add_new_inode_to_cache);
    run_test(iupdate_correctness);
    run_test(idup_correctness);
    run_test(ilock_read_from_disk);
    run_test(ilock_read_from_disk_invalid_inum);
    run_test(iput_no_links);
    run_test(iput_with_links);
    run_test(stati_correctness_data_object_no_exists);
    run_test(stati_correctness_data_object_exists);
    run_test(readi_from_existing_without_offset);
    run_test(readi_from_existing_with_offset);
    run_test(readi_from_non_existing);
    run_test(readi_read_too_much);
    run_test(readi_read_offset_too_large);
    run_test(writei_from_non_existing);
    run_test(writei_from_existing_without_offset);
    run_test(writei_from_existing_with_offset);
    run_test(writei_write_beyond_fail);
    run_test(writei_write_append);

    // Summary
    printf("[===========]\n");
    if (failed) {
        printf("[  FAILED   ]\n");
    } else {
        printf("[    PASS   ]\n");
    }
    printf("[===========]\n");
    return failed;
}
