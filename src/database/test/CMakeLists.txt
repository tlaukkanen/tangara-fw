idf_component_register(
  SRCS "test_records.cpp" "test_database.cpp"
  INCLUDE_DIRS "."
  REQUIRES catch2 cmock database drivers fixtures)
