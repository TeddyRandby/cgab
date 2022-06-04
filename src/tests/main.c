#include "../gab/gab.h"
#include <check.h>

static gab_engine *gab = NULL;

void setup_engine() { gab = gab_create(); }

void teardown_engine() { gab_destroy(gab); }

#define FILE_TEST(name, path, expected_result)                                 \
  START_TEST(name) {                                                           \
    s_u8 *file = gab_io_read_file(GAB_TEST_DIR path);                          \
    gab_result *result =                                                       \
        gab_run_source(gab, (char *)file->data, #name, GAB_FLAG_NONE);         \
    ck_assert(result->type == expected_result);                                \
    gab_result_destroy(result);                                                \
  }                                                                            \
  END_TEST;

#define FILE_TEST_VALUE(name, path, expected_value)                            \
  START_TEST(name) {                                                           \
    s_u8 *file = gab_io_read_file(GAB_TEST_DIR path);                          \
    gab_result *result =                                                       \
        gab_run_source(gab, (char *)file->data, #name, GAB_FLAG_NONE);         \
    ck_assert(result->type == RESULT_RUN_SUCCESS);                             \
    ck_assert(gab_val_equal(expected_value, result->as.result));               \
    gab_result_destroy(result);                                                \
  }                                                                            \
  END_TEST;

FILE_TEST(too_many_locals, "/compile_fail/too_many_locals.gab",
          RESULT_COMPILE_FAIL);
FILE_TEST(too_many_fargs, "/compile_fail/too_many_fargs.gab",
          RESULT_COMPILE_FAIL);
FILE_TEST(too_many_returned, "/compile_fail/too_many_returned.gab",
          RESULT_COMPILE_FAIL);
FILE_TEST(missing_do_end, "/compile_fail/missing_do_end.gab",
          RESULT_COMPILE_FAIL);
FILE_TEST(missing_str_end, "/compile_fail/missing_str_end.gab",
          RESULT_COMPILE_FAIL);
FILE_TEST(missing_var_in_init, "/compile_fail/var_in_initializer.gab",
          RESULT_COMPILE_FAIL);
FILE_TEST(var_with_name, "/compile_fail/var_with_name.gab",
          RESULT_COMPILE_FAIL);
FILE_TEST(too_many_let, "/compile_fail/too_many_let.gab", RESULT_COMPILE_FAIL);
FILE_TEST(shadow, "/compile_fail/shadow.gab", RESULT_COMPILE_FAIL);
FILE_TEST(too_many_exp, "/compile_fail/too_many_exp.gab", RESULT_COMPILE_FAIL);
FILE_TEST(too_many_lst, "/compile_fail/too_many_lst.gab", RESULT_COMPILE_FAIL);
FILE_TEST(too_many_obj, "/compile_fail/too_many_obj.gab", RESULT_COMPILE_FAIL);
FILE_TEST(missing_id, "/compile_fail/missing_id.gab", RESULT_COMPILE_FAIL);
FILE_TEST(bad_assign, "/compile_fail/bad_assign.gab", RESULT_COMPILE_FAIL);
FILE_TEST(bad_index, "/compile_fail/bad_index.gab", RESULT_COMPILE_FAIL);

Suite *compile_fail_suite() {
  Suite *s;
  TCase *tc_core;

  s = suite_create("Compile Fail Suite");
  tc_core = tcase_create("Core");

  tcase_add_checked_fixture(tc_core, setup_engine, teardown_engine);

  tcase_add_test(tc_core, too_many_locals);
  tcase_add_test(tc_core, too_many_fargs);
  tcase_add_test(tc_core, too_many_returned);
  tcase_add_test(tc_core, too_many_exp);
  tcase_add_test(tc_core, too_many_lst);
  tcase_add_test(tc_core, too_many_obj);
  tcase_add_test(tc_core, too_many_let);
  tcase_add_test(tc_core, missing_do_end);
  tcase_add_test(tc_core, missing_str_end);
  tcase_add_test(tc_core, missing_var_in_init);
  tcase_add_test(tc_core, missing_id);
  tcase_add_test(tc_core, var_with_name);
  tcase_add_test(tc_core, shadow);
  tcase_add_test(tc_core, bad_assign);
  tcase_add_test(tc_core, bad_index);

  suite_add_tcase(s, tc_core);

  return s;
};

FILE_TEST(bad_bin, "/run_fail/bad_bin.gab", RESULT_RUN_FAIL);
FILE_TEST(bad_concat, "/run_fail/bad_concat.gab", RESULT_RUN_FAIL);
FILE_TEST(bad_call, "/run_fail/bad_call.gab", RESULT_RUN_FAIL);
FILE_TEST(bad_args, "/run_fail/bad_args.gab", RESULT_RUN_FAIL);
FILE_TEST(bad_prop, "/run_fail/bad_prop.gab", RESULT_RUN_FAIL);

Suite *run_fail_suite() {
  Suite *s;
  TCase *tc_core;

  s = suite_create("Run Fail Suite");
  tc_core = tcase_create("Core");

  tcase_add_checked_fixture(tc_core, setup_engine, teardown_engine);

  tcase_add_test(tc_core, bad_bin);
  tcase_add_test(tc_core, bad_concat);
  tcase_add_test(tc_core, bad_call);
  tcase_add_test(tc_core, bad_args);
  tcase_add_test(tc_core, bad_prop);

  suite_add_tcase(s, tc_core);

  return s;
}

FILE_TEST_VALUE(exp_blk, "/expression/blk.gab", GAB_VAL_NUMBER(2));
FILE_TEST_VALUE(exp_if_true, "/expression/if_true.gab", GAB_VAL_NUMBER(1));
FILE_TEST_VALUE(exp_if_false, "/expression/if_false.gab", GAB_VAL_NULL());
FILE_TEST_VALUE(exp_if_else, "/expression/if_else.gab", GAB_VAL_NUMBER(1));
FILE_TEST_VALUE(exp_if_true_blk, "/expression/if_true_blk.gab",
                GAB_VAL_NUMBER(1));
FILE_TEST_VALUE(exp_if_false_blk, "/expression/if_false_blk.gab",
                GAB_VAL_NULL());
FILE_TEST_VALUE(exp_if_else_blk, "/expression/if_else_blk.gab",
                GAB_VAL_NUMBER(1));

Suite *expression_suite() {
  Suite *s;
  TCase *tc_core;

  s = suite_create("Expression Suite");
  tc_core = tcase_create("Core");

  tcase_add_checked_fixture(tc_core, setup_engine, teardown_engine);

  tcase_add_test(tc_core, exp_blk);
  tcase_add_test(tc_core, exp_if_true);
  tcase_add_test(tc_core, exp_if_false);
  tcase_add_test(tc_core, exp_if_else);
  tcase_add_test(tc_core, exp_if_true_blk);
  tcase_add_test(tc_core, exp_if_false_blk);
  tcase_add_test(tc_core, exp_if_else_blk);

  suite_add_tcase(s, tc_core);
  return s;
}

i32 main(i32 argc, const char *argv[]) {
  u64 fails = 0;
  Suite *s;
  SRunner *sr;

  s = compile_fail_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  fails += srunner_ntests_failed(sr);
  srunner_free(sr);

  s = run_fail_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  fails += srunner_ntests_failed(sr);
  srunner_free(sr);

  s = expression_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  fails += srunner_ntests_failed(sr);
  srunner_free(sr);

  return (fails == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
};
