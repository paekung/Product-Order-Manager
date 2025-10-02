#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

// Dedicated unit tests for add_product and update_product helpers.
#define TEST_PRODUCTS_FILE "products.csv"

typedef struct {
    char ProductID[20];
    char ProductName[100];
    int Quantity;
    int UnitPrice;
} Product;

extern Product *products;
extern int product_count;
extern int product_capacity;

int add_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice);
int update_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice);

typedef struct {
    Product *original_products;
    Product *snapshot;
    int original_count;
    int original_capacity;
} ProductStateBackup;

typedef struct {
    char *data;
    size_t size;
    int existed;
} FileBackup;

// Capture global buffers so every test starts from a clean slate.
static int backup_product_state(ProductStateBackup *backup) {
    if (!backup) {
        return -1;
    }

    backup->original_products = products;
    backup->original_count = product_count;
    backup->original_capacity = product_capacity;
    backup->snapshot = NULL;

    if (products && product_capacity > 0) {
        backup->snapshot = (Product *)malloc((size_t)product_capacity * sizeof(Product));
        if (!backup->snapshot) {
            return -1;
        }
        memcpy(backup->snapshot, products, (size_t)product_capacity * sizeof(Product));
    }

    products = NULL;
    product_count = 0;
    product_capacity = 0;

    return 0;
}

static void restore_product_state(ProductStateBackup *backup) {
    if (!backup) {
        return;
    }

    free(products);
    products = backup->original_products;
    product_count = backup->original_count;
    product_capacity = backup->original_capacity;

    if (backup->snapshot && backup->original_products && backup->original_capacity > 0) {
        memcpy(backup->original_products,
               backup->snapshot,
               (size_t)backup->original_capacity * sizeof(Product));
    }

    free(backup->snapshot);
    backup->snapshot = NULL;
}

static void reset_test_environment(void) {
    free(products);
    products = NULL;
    product_count = 0;
    product_capacity = 0;
}

// Preserve the on-disk catalog to avoid clobbering user data while testing.
static int backup_products_file(FileBackup *backup, const char *path) {
    if (!backup || !path) {
        return -1;
    }

    backup->data = NULL;
    backup->size = 0;
    backup->existed = 0;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (errno == ENOENT) {
            return 0;
        }
        return -1;
    }

    backup->existed = 1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    backup->size = (size_t)len;
    if (backup->size > 0) {
        backup->data = (char *)malloc(backup->size);
        if (!backup->data) {
            fclose(fp);
            return -1;
        }
        size_t read_bytes = fread(backup->data, 1, backup->size, fp);
        if (read_bytes != backup->size) {
            free(backup->data);
            backup->data = NULL;
            backup->size = 0;
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

static int restore_products_file(FileBackup *backup, const char *path) {
    if (!backup || !path) {
        return -1;
    }

    int result = 0;

    if (backup->existed) {
        FILE *fp = fopen(path, "wb");
        if (!fp) {
            result = -1;
            goto cleanup;
        }
        if (backup->size > 0) {
            size_t written = fwrite(backup->data, 1, backup->size, fp);
            if (written != backup->size) {
                fclose(fp);
                result = -1;
                goto cleanup;
            }
        }
        if (fclose(fp) != 0) {
            result = -1;
        }
    } else {
        if (remove(path) != 0 && errno != ENOENT) {
            result = -1;
        }
    }

cleanup:
    free(backup->data);
    backup->data = NULL;
    backup->size = 0;
    backup->existed = 0;
    return result;
}

static int test_add_product_inserts_new_entry(void) {
    int rc = add_product("UT001", "Test Widget", 5, 100);
    if (rc != 0) {
        printf("    Expected add_product success, got %d\n", rc);
        return 1;
    }
    if (product_count != 1) {
        printf("    Expected product_count 1, got %d\n", product_count);
        return 1;
    }
    if (product_capacity < 1) {
        printf("    Expected product_capacity >= 1, got %d\n", product_capacity);
        return 1;
    }
    if (!products) {
        printf("    Products array is NULL after insertion\n");
        return 1;
    }
    Product *added = &products[0];
    if (strcmp(added->ProductID, "UT001") != 0) {
        printf("    ProductID mismatch: %s\n", added->ProductID);
        return 1;
    }
    if (strcmp(added->ProductName, "Test Widget") != 0) {
        printf("    ProductName mismatch: %s\n", added->ProductName);
        return 1;
    }
    if (added->Quantity != 5 || added->UnitPrice != 100) {
        printf("    Quantity/UnitPrice mismatch: %d/%d\n", added->Quantity, added->UnitPrice);
        return 1;
    }
    return 0;
}

static int test_add_product_rejects_duplicate_id(void) {
    int rc = add_product("UT002", "Initial", 10, 200);
    if (rc != 0) {
        printf("    Failed to seed product for duplicate test\n");
        return 1;
    }
    rc = add_product("UT002", "Duplicate", 5, 50);
    if (rc != 1) {
        printf("    Expected duplicate add_product to return 1, got %d\n", rc);
        return 1;
    }
    if (product_count != 1) {
        printf("    Product count changed after duplicate attempt: %d\n", product_count);
        return 1;
    }
    if (strcmp(products[0].ProductName, "Initial") != 0 ||
        products[0].Quantity != 10 ||
        products[0].UnitPrice != 200) {
        printf("    Existing product mutated after duplicate attempt\n");
        return 1;
    }
    return 0;
}

static int test_add_product_expands_capacity(void) {
    const int to_insert = 15; // exceeds default allocation (10)
    for (int i = 0; i < to_insert; i++) {
        char id[20];
        char name[100];
        snprintf(id, sizeof(id), "CAP%03d", i);
        snprintf(name, sizeof(name), "Capacity Test %d", i);
        if (add_product(id, name, i, i * 10) != 0) {
            printf("    add_product failed at index %d\n", i);
            return 1;
        }
    }

    if (product_count != to_insert) {
        printf("    Expected %d products, got %d\n", to_insert, product_count);
        return 1;
    }
    if (product_capacity < to_insert) {
        printf("    Expected product_capacity >= %d, got %d\n", to_insert, product_capacity);
        return 1;
    }

    Product *last = &products[to_insert - 1];
    if (strcmp(last->ProductID, "CAP014") != 0 ||
        strcmp(last->ProductName, "Capacity Test 14") != 0 ||
        last->Quantity != 14 ||
        last->UnitPrice != 140) {
        printf("    Last product data incorrect after expansion\n");
        return 1;
    }

    return 0;
}

static int test_add_product_accepts_zero_values(void) {
    int rc = add_product("UT011", "ZeroCase", 0, 0);
    if (rc != 0) {
        printf("    Expected add_product success with zero values, got %d\n", rc);
        return 1;
    }
    if (product_count != 1) {
        printf("    Expected product_count 1, got %d\n", product_count);
        return 1;
    }
    if (!products) {
        printf("    Products array is NULL after zero-value insertion\n");
        return 1;
    }
    if (products[0].Quantity != 0 || products[0].UnitPrice != 0) {
        printf("    Zero values not stored correctly: %d/%d\n", products[0].Quantity, products[0].UnitPrice);
        return 1;
    }
    return 0;
}

static int test_add_product_handles_large_values(void) {
    int rc = add_product("UT012", "MaxCase", INT_MAX, INT_MAX);
    if (rc != 0) {
        printf("    Expected add_product success with INT_MAX values, got %d\n", rc);
        return 1;
    }
    if (product_count != 1) {
        printf("    Expected product_count 1, got %d\n", product_count);
        return 1;
    }
    if (!products) {
        printf("    Products array is NULL after INT_MAX insertion\n");
        return 1;
    }
    if (products[0].Quantity != INT_MAX || products[0].UnitPrice != INT_MAX) {
        printf("    INT_MAX values not stored correctly: %d/%d\n", products[0].Quantity, products[0].UnitPrice);
        return 1;
    }
    return 0;
}

static int test_add_product_rejects_empty_name(void) {
    int rc = add_product("UT013", "", 1, 1);
    if (rc != 1) {
        printf("    Expected add_product to reject empty name, got %d\n", rc);
        return 1;
    }
    if (product_count != 0) {
        printf("    Product count should remain 0 after rejection, got %d\n", product_count);
        return 1;
    }
    if (products != NULL) {
        printf("    Products array should remain NULL after rejection\n");
        return 1;
    }
    return 0;
}

static int test_add_product_rejects_whitespace_name(void) {
    int rc = add_product("UT014", "   \t", 1, 1);
    if (rc != 1) {
        printf("    Expected add_product to reject whitespace name, got %d\n", rc);
        return 1;
    }
    if (product_count != 0) {
        printf("    Product count should remain 0 after whitespace rejection, got %d\n", product_count);
        return 1;
    }
    if (products != NULL) {
        printf("    Products array should remain NULL after whitespace rejection\n");
        return 1;
    }
    return 0;
}

static int test_add_product_handles_max_length_strings(void) {
    char long_id[20];
    char long_name[100];
    for (int i = 0; i < 19; i++) {
        long_id[i] = (char)('A' + (i % 26));
    }
    long_id[19] = '\0';

    for (int i = 0; i < 99; i++) {
        long_name[i] = (char)('a' + (i % 26));
    }
    long_name[99] = '\0';

    int rc = add_product(long_id, long_name, 9, 99);
    if (rc != 0) {
        printf("    Expected add_product success with max-length strings, got %d\n", rc);
        return 1;
    }
    if (product_count != 1) {
        printf("    Expected product_count 1, got %d\n", product_count);
        return 1;
    }
    if (!products) {
        printf("    Products array is NULL after max-length insertion\n");
        return 1;
    }
    if (strcmp(products[0].ProductID, long_id) != 0) {
        printf("    ProductID not preserved for max-length case\n");
        return 1;
    }
    if (strcmp(products[0].ProductName, long_name) != 0) {
        printf("    ProductName not preserved for max-length case\n");
        return 1;
    }
    if (products[0].Quantity != 9 || products[0].UnitPrice != 99) {
        printf("    Quantity/UnitPrice not stored correctly: %d/%d\n", products[0].Quantity, products[0].UnitPrice);
        return 1;
    }
    return 0;
}

static int test_add_product_persists_to_csv(void) {
    int rc = add_product("UT010", "Persist", 3, 30);
    if (rc != 0) {
        printf("    Expected add_product success, got %d\n", rc);
        return 1;
    }

    FILE *fp = fopen(TEST_PRODUCTS_FILE, "r");
    if (!fp) {
        printf("    Failed to open %s for verification\n", TEST_PRODUCTS_FILE);
        return 1;
    }

    char line[256];
    if (!fgets(line, sizeof(line), fp)) {
        printf("    Missing CSV header\n");
        fclose(fp);
        return 1;
    }
    const char *expected_header = "ProductID,ProductName,Quantity,UnitPrice";
    line[strcspn(line, "\r\n")] = '\0';
    if (strcmp(line, expected_header) != 0) {
        printf("    CSV header mismatch: %s\n", line);
        fclose(fp);
        return 1;
    }

    if (!fgets(line, sizeof(line), fp)) {
        printf("    Missing CSV detail row\n");
        fclose(fp);
        return 1;
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (strcmp(line, "UT010,Persist,3,30") != 0) {
        printf("    CSV row mismatch: %s\n", line);
        fclose(fp);
        return 1;
    }

    if (fgets(line, sizeof(line), fp)) {
        printf("    Unexpected extra CSV rows\n");
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0;
}

static int test_add_product_appends_after_manual_seed(void) {
    products = (Product *)malloc(3 * sizeof(Product));
    if (!products) {
        printf("    Memory allocation failed\n");
        return 1;
    }
    product_capacity = 3;
    product_count = 2;

    strcpy(products[0].ProductID, "UT020");
    strcpy(products[0].ProductName, "SeedOne");
    products[0].Quantity = 2;
    products[0].UnitPrice = 20;

    strcpy(products[1].ProductID, "UT021");
    strcpy(products[1].ProductName, "SeedTwo");
    products[1].Quantity = 4;
    products[1].UnitPrice = 40;

    int add_rc = add_product("UT022", "SeedThree", 6, 60);
    if (add_rc != 0) {
        printf("    add_product failed while appending: %d\n", add_rc);
        return 1;
    }
    if (product_count != 3) {
        printf("    Expected product_count 3, got %d\n", product_count);
        return 1;
    }
    if (product_capacity != 3) {
        printf("    Expected product_capacity to remain 3, got %d\n", product_capacity);
        return 1;
    }
    if (strcmp(products[0].ProductName, "SeedOne") != 0 ||
        strcmp(products[1].ProductName, "SeedTwo") != 0) {
        printf("    Existing products mutated during append\n");
        return 1;
    }
    Product *added = &products[2];
    if (strcmp(added->ProductID, "UT022") != 0 ||
        strcmp(added->ProductName, "SeedThree") != 0 ||
        added->Quantity != 6 ||
        added->UnitPrice != 60) {
        printf("    Appended product incorrect\n");
        return 1;
    }

    return 0;
}

static int test_update_product_changes_fields(void) {
    products = (Product *)malloc(sizeof(Product));
    if (!products) {
        printf("    Memory allocation failed\n");
        return 1;
    }
    product_capacity = 1;
    product_count = 1;
    strcpy(products[0].ProductID, "UT003");
    strcpy(products[0].ProductName, "Original");
    products[0].Quantity = 1;
    products[0].UnitPrice = 10;

    int rc = update_product("UT003", "Updated", 25, 500);
    if (rc != 0) {
        printf("    Expected update_product success, got %d\n", rc);
        return 1;
    }
    if (strcmp(products[0].ProductName, "Updated") != 0) {
        printf("    ProductName not updated: %s\n", products[0].ProductName);
        return 1;
    }
    if (products[0].Quantity != 25 || products[0].UnitPrice != 500) {
        printf("    Quantity/UnitPrice not updated: %d/%d\n", products[0].Quantity, products[0].UnitPrice);
        return 1;
    }
    if (strcmp(products[0].ProductID, "UT003") != 0) {
        printf("    ProductID unexpectedly changed\n");
        return 1;
    }
    return 0;
}

static int test_update_product_handles_partial_updates(void) {
    products = (Product *)malloc(sizeof(Product));
    if (!products) {
        printf("    Memory allocation failed\n");
        return 1;
    }
    product_capacity = 1;
    product_count = 1;
    strcpy(products[0].ProductID, "UT004");
    strcpy(products[0].ProductName, "KeepName");
    products[0].Quantity = 7;
    products[0].UnitPrice = 70;

    int rc = update_product("UT004", NULL, -1, 90);
    if (rc != 0) {
        printf("    Expected update_product success, got %d\n", rc);
        return 1;
    }
    if (strcmp(products[0].ProductName, "KeepName") != 0) {
        printf("    ProductName should remain unchanged\n");
        return 1;
    }
    if (products[0].Quantity != 7) {
        printf("    Quantity should remain unchanged: %d\n", products[0].Quantity);
        return 1;
    }
    if (products[0].UnitPrice != 90) {
        printf("    UnitPrice should update to 90: %d\n", products[0].UnitPrice);
        return 1;
    }
    return 0;
}

static int test_update_product_missing_id_fails(void) {
    products = (Product *)malloc(sizeof(Product));
    if (!products) {
        printf("    Memory allocation failed\n");
        return 1;
    }
    product_capacity = 1;
    product_count = 1;
    strcpy(products[0].ProductID, "UT005");
    strcpy(products[0].ProductName, "Original");
    products[0].Quantity = 1;
    products[0].UnitPrice = 10;

    int rc = update_product("UNKNOWN", "Won't Matter", 2, 20);
    if (rc != 1) {
        printf("    Expected update_product to fail for missing ID, got %d\n", rc);
        return 1;
    }
    if (strcmp(products[0].ProductName, "Original") != 0 || products[0].Quantity != 1 || products[0].UnitPrice != 10) {
        printf("    Product data changed unexpectedly\n");
        return 1;
    }
    return 0;
}

static int test_update_product_empty_inventory_fails(void) {
    int rc = update_product("UT999", "Nope", 1, 1);
    if (rc != 1) {
        printf("    Expected failure when inventory empty, got %d\n", rc);
        return 1;
    }
    if (products != NULL || product_count != 0 || product_capacity != 0) {
        printf("    Inventory state should remain empty\n");
        return 1;
    }
    return 0;
}

static int test_update_product_handles_max_values(void) {
    products = (Product *)malloc(sizeof(Product));
    if (!products) {
        printf("    Memory allocation failed\n");
        return 1;
    }
    product_capacity = 1;
    product_count = 1;
    strcpy(products[0].ProductID, "UT033");
    strcpy(products[0].ProductName, "MaxTarget");
    products[0].Quantity = 1;
    products[0].UnitPrice = 1;

    int rc = update_product("UT033", "MaxTarget", INT_MAX, INT_MAX);
    if (rc != 0) {
        printf("    Expected update_product success with INT_MAX values, got %d\n", rc);
        return 1;
    }
    if (strcmp(products[0].ProductName, "MaxTarget") != 0) {
        printf("    Product name should remain MaxTarget\n");
        return 1;
    }
    if (products[0].Quantity != INT_MAX || products[0].UnitPrice != INT_MAX) {
        printf("    INT_MAX values not stored correctly during update: %d/%d\n", products[0].Quantity, products[0].UnitPrice);
        return 1;
    }
    return 0;
}

static int test_update_product_rejects_negative_numbers(void) {
    products = (Product *)malloc(sizeof(Product));
    if (!products) {
        printf("    Memory allocation failed\n");
        return 1;
    }
    product_capacity = 1;
    product_count = 1;
    strcpy(products[0].ProductID, "UT034");
    strcpy(products[0].ProductName, "NegTarget");
    products[0].Quantity = 12;
    products[0].UnitPrice = 120;

    int rc = update_product("UT034", "NegTarget", -10, -20);
    if (rc != 0) {
        printf("    Expected update_product success when skipping negative updates, got %d\n", rc);
        return 1;
    }
    if (strcmp(products[0].ProductName, "NegTarget") != 0) {
        printf("    Product name should remain unchanged\n");
        return 1;
    }
    if (products[0].Quantity != 12 || products[0].UnitPrice != 120) {
        printf("    Negative inputs should not modify values: %d/%d\n", products[0].Quantity, products[0].UnitPrice);
        return 1;
    }
    return 0;
}

static int test_update_product_sets_zero_values(void) {
    products = (Product *)malloc(sizeof(Product));
    if (!products) {
        printf("    Memory allocation failed\n");
        return 1;
    }
    product_capacity = 1;
    product_count = 1;
    strcpy(products[0].ProductID, "UT035");
    strcpy(products[0].ProductName, "ZeroUpdate");
    products[0].Quantity = 15;
    products[0].UnitPrice = 150;

    int rc = update_product("UT035", "ZeroUpdate", 0, 0);
    if (rc != 0) {
        printf("    Expected update_product success when setting zeros, got %d\n", rc);
        return 1;
    }
    if (products[0].Quantity != 0 || products[0].UnitPrice != 0) {
        printf("    Zero update did not apply: %d/%d\n", products[0].Quantity, products[0].UnitPrice);
        return 1;
    }
    if (strcmp(products[0].ProductName, "ZeroUpdate") != 0) {
        printf("    Product name should remain unchanged\n");
        return 1;
    }
    return 0;
}

static int test_update_product_preserves_other_records(void) {
    products = (Product *)malloc(2 * sizeof(Product));
    if (!products) {
        printf("    Memory allocation failed\n");
        return 1;
    }
    product_capacity = 2;
    product_count = 2;

    strcpy(products[0].ProductID, "UT030");
    strcpy(products[0].ProductName, "Primary");
    products[0].Quantity = 11;
    products[0].UnitPrice = 110;

    strcpy(products[1].ProductID, "UT031");
    strcpy(products[1].ProductName, "Secondary");
    products[1].Quantity = 22;
    products[1].UnitPrice = 220;

    int rc = update_product("UT031", "SecondaryUpdated", 33, 330);
    if (rc != 0) {
        printf("    Expected update_product success, got %d\n", rc);
        return 1;
    }

    if (strcmp(products[0].ProductName, "Primary") != 0 ||
        products[0].Quantity != 11 ||
        products[0].UnitPrice != 110) {
        printf("    Non-target product mutated\n");
        return 1;
    }

    if (strcmp(products[1].ProductName, "SecondaryUpdated") != 0 ||
        products[1].Quantity != 33 ||
        products[1].UnitPrice != 330) {
        printf("    Target product not updated correctly\n");
        return 1;
    }

    return 0;
}

static int test_update_product_rejects_empty_name(void) {
    products = (Product *)malloc(sizeof(Product));
    if (!products) {
        printf("    Memory allocation failed\n");
        return 1;
    }
    product_capacity = 1;
    product_count = 1;
    strcpy(products[0].ProductID, "UT032");
    strcpy(products[0].ProductName, "NonEmpty");
    products[0].Quantity = 5;
    products[0].UnitPrice = 50;

    int rc = update_product("UT032", "", 8, 80);
    if (rc != 1) {
        printf("    Expected update_product to reject empty name, got %d\n", rc);
        return 1;
    }

    if (strcmp(products[0].ProductName, "NonEmpty") != 0) {
        printf("    ProductName should remain unchanged after rejection\n");
        return 1;
    }
    if (products[0].Quantity != 5 || products[0].UnitPrice != 50) {
        printf("    Quantity/UnitPrice should remain unchanged after rejection: %d/%d\n", products[0].Quantity, products[0].UnitPrice);
        return 1;
    }

    return 0;
}

typedef int (*TestFunc)(void);

typedef struct {
    const char *name;
    TestFunc func;
} TestCase;

int run_unit_tests(void) {
    ProductStateBackup state_backup;
    FileBackup file_backup;
    if (backup_product_state(&state_backup) != 0) {
        printf("Failed to back up product state.\n");
        return 1;
    }

    if (backup_products_file(&file_backup, TEST_PRODUCTS_FILE) != 0) {
        printf("Failed to back up %s.\n", TEST_PRODUCTS_FILE);
        restore_product_state(&state_backup);
        return 1;
    }

    const TestCase tests[] = {
        {"add_product inserts new entry", test_add_product_inserts_new_entry},
        {"add_product rejects duplicate IDs", test_add_product_rejects_duplicate_id},
        {"add_product expands capacity", test_add_product_expands_capacity},
        {"add_product accepts zero values", test_add_product_accepts_zero_values},
        {"add_product handles INT_MAX values", test_add_product_handles_large_values},
        {"add_product rejects empty name", test_add_product_rejects_empty_name},
        {"add_product rejects whitespace name", test_add_product_rejects_whitespace_name},
        {"add_product handles max-length strings", test_add_product_handles_max_length_strings},
        {"add_product persists data to CSV", test_add_product_persists_to_csv},
        {"add_product appends after manual seed", test_add_product_appends_after_manual_seed},
        {"update_product changes all fields", test_update_product_changes_fields},
        {"update_product supports partial updates", test_update_product_handles_partial_updates},
        {"update_product fails for missing ID", test_update_product_missing_id_fails},
        {"update_product fails with empty inventory", test_update_product_empty_inventory_fails},
        {"update_product handles INT_MAX values", test_update_product_handles_max_values},
        {"update_product ignores negative numbers", test_update_product_rejects_negative_numbers},
        {"update_product sets zero values", test_update_product_sets_zero_values},
        {"update_product preserves other records", test_update_product_preserves_other_records},
        {"update_product rejects empty name", test_update_product_rejects_empty_name}
    };

    const size_t total_tests = sizeof(tests) / sizeof(tests[0]);
    size_t passed_tests = 0;

    printf("Running unit tests...\n\n");

    for (size_t i = 0; i < total_tests; i++) {
        reset_test_environment();
        int rc = tests[i].func();
        if (rc == 0) {
            printf("\033[1;32m[PASS]\033[0m %s\n", tests[i].name);
            passed_tests++;
        } else {
            printf("\033[31m[FAIL]\033[0m %s\n", tests[i].name);
        }
    }

    reset_test_environment();

    if (restore_products_file(&file_backup, TEST_PRODUCTS_FILE) != 0) {
        printf("Warning: Failed to restore %s.\n", TEST_PRODUCTS_FILE);
    }

    restore_product_state(&state_backup);

    printf("\nTest summary: %zu/%zu passed.\n", passed_tests, total_tests);

    return (passed_tests == total_tests) ? 0 : 1;
}
