include(dtest)

# ------------------------------ Range and Slice -------------------------------

add_dtests(NAME test_range FILES test_range.cpp LIBS parlay)
add_dtests(NAME test_slice FILES test_slice.cpp LIBS parlay)

# ------------------------------ Data Structures -------------------------------

add_dtests(NAME test_delayed_sequence FILES test_delayed_sequence.cpp LIBS parlay)
add_dtests(NAME test_sequence FILES test_sequence.cpp LIBS parlay)
add_dtests(NAME test_hash_table FILES test_hash_table.cpp LIBS parlay)

# ----------------------------- Sorting Algorithms ------------------------------

add_dtests(NAME test_merge_sort FILES test_merge_sort.cpp LIBS parlay)
add_dtests(NAME test_quicksort FILES test_quicksort.cpp LIBS parlay)
add_dtests(NAME test_bucket_sort FILES test_bucket_sort.cpp LIBS parlay)
add_dtests(NAME test_integer_sort FILES test_integer_sort.cpp LIBS parlay)
add_dtests(NAME test_counting_sort FILES test_counting_sort.cpp LIBS parlay)
add_dtests(NAME test_sample_sort FILES test_sample_sort.cpp LIBS parlay)

# -------------------------------- Primitives ---------------------------------

add_dtests(NAME test_primitives FILES test_primitives.cpp LIBS parlay)
add_dtests(NAME test_random FILES test_random.cpp LIBS parlay)

# -------------------------- Uninitialized memory testing ---------------------------

add_dtests(NAME test_uninitialized_memory FILES test_uninitialized_memory.cpp LIBS parlay NO_SANITIZE)

# -------------------------------- IO ---------------------------------

add_dtests(NAME test_io FILES test_io.cpp LIBS parlay)
add_dtests(NAME test_file_map FILES test_file_map.cpp LIBS parlay)
add_dtests(NAME test_file_map_fallback FILES test_file_map.cpp LIBS parlay FLAGS "-DPARLAY_USE_FALLBACK_FILE_MAP")

# --------------------------- Parsing and Formatting ----------------------------

if(NOT WSL)
  add_dtests(NAME test_parsing FILES test_parsing.cpp LIBS parlay)
endif()

add_dtests(NAME test_formatting FILES test_formatting.cpp LIBS parlay)

# ----------------------------- Parlay Scheduling ------------------------------

add_dtests(NAME test_scheduler FILES test_scheduler.cpp LIBS parlay)
add_dtests(NAME test_parlay_plugin FILES test_parlay_scheduler_plugin.cpp LIBS parlay)
add_dtests(NAME test_seq_scheduling FILES test_seq_scheduling.cpp LIBS parlay)

# ----------------------------- Parlay Allocator ------------------------------

add_dtests(NAME test_allocator FILES test_allocator.cpp LIBS parlay)

# ----------------------------- Utilities ------------------------------

add_dtests(NAME test_relocate FILES test_relocate.cpp LIBS parlay)

# --------------------- External scheduler integration tests -------------------

# Cilk integration test (requires a Cilk-compatible compiler)
set(OPENCILK_FLAGS -fopencilk)
set(CILKPLUS_FLAGS -fcilkplus)
option(PARLAY_TEST_CILK "Enable unit test for Cilk integration (requires a Cilk-compatible compiler)" Off)
if(PARLAY_TEST_CILK)
  flags_supported(${OPENCILK_FLAGS} OPENCILK_SUPPORT)
  if (OPENCILK_SUPPORT)
    set(CILK_FLAGS ${OPENCILK_FLAGS})
  else()
    # icpc 12.x.x.x--19.x.x.x support Cilk Plus (but do not take -fcilkplus)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Intel"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12.0.0.0"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "20.0.0.0")
      set(CILKPLUS_SUPPORT 1)
    else()
      flags_supported(${CILKPLUS_FLAGS} CILKPLUS_SUPPORT)
      if (CILKPLUS_SUPPORT)
        set(CILK_FLAGS ${CILKPLUS_FLAGS})
      endif()
    endif()
  endif()
  if(OPENCILK_SUPPORT OR CILKPLUS_SUPPORT)
    add_dtests(NAME test_cilk_plugin FILES test_cilk_plugin.cpp LIBS parlay FLAGS ${CILK_FLAGS})
  else()
    message(FATAL_ERROR "You are trying to build the Cilk integration tests, but your compiler does not support Cilk. You'll need to switch to a compiler that does, or turn off the Cilk tests (-DTEST_CILK=Off)")
  endif()
endif()

# TBB integration test (requires TBB installed)
option(PARLAY_TEST_TBB "Enable unit test for TBB integration (requires TBB to be installed and on the library/include path" Off)
if (PARLAY_TEST_TBB)
  add_dtests(NAME test_tbb_plugin FILES test_tbb_plugin.cpp LIBS parlay tbb)
endif()

# OMP integration test (requires OMP installed)
option(PARLAY_TEST_OMP "Enable unit test for OpenMP integration (requires a compatible installation of OpenMP for your compiler" Off)
if (PARLAY_TEST_OMP)
  add_dtests(NAME test_omp_plugin FILES test_omp_plugin.cpp LIBS parlay FLAGS -fopenmp)
endif()
