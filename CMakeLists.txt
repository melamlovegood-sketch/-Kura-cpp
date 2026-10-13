cmake_minimum_required(VERSION 3.16)
project(KuraCpp VERSION 0.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

add_library(kura_core STATIC
    src/Transaction.cpp src/Transaction.h
    src/DatabaseManager.cpp src/DatabaseManager.h
    src/TransactionRepository.cpp src/TransactionRepository.h
    src/TransactionFilter.cpp src/TransactionFilter.h
    src/ExpenseSummary.cpp src/ExpenseSummary.h
    src/CategorySummary.cpp src/CategorySummary.h
    src/MonthlyBudget.cpp src/MonthlyBudget.h
    src/BudgetRepository.cpp src/BudgetRepository.h
    src/ExpenseStatistics.cpp src/ExpenseStatistics.h
    src/WishItem.cpp src/WishItem.h
    src/WishlistRepository.cpp src/WishlistRepository.h
    src/CsvExporter.cpp src/CsvExporter.h
    src/DatabaseBackupService.cpp src/DatabaseBackupService.h
    src/DemoData.cpp src/DemoData.h
)
target_include_directories(kura_core PUBLIC src)
target_link_libraries(kura_core PUBLIC Qt6::Sql)

add_library(kura_widgets STATIC
    src/mainwindow.cpp src/mainwindow.h
    src/AddTransactionDialog.cpp src/AddTransactionDialog.h
    src/BudgetWidget.cpp src/BudgetWidget.h
    src/MonthlyTrendChart.cpp src/MonthlyTrendChart.h
    src/StatisticsWidget.cpp src/StatisticsWidget.h
    src/WishDialog.cpp src/WishDialog.h
    src/WishlistWidget.cpp src/WishlistWidget.h
    src/DataManagementWidget.cpp src/DataManagementWidget.h
    src/AppTheme.cpp src/AppTheme.h
)
target_link_libraries(kura_widgets PUBLIC kura_core Qt6::Widgets)
add_executable(KuraCpp WIN32 src/main.cpp)
target_link_libraries(KuraCpp PRIVATE kura_widgets)
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(category_summary_tests tests/category_summary_tests.cpp)
    target_link_libraries(category_summary_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME category_summary_tests COMMAND category_summary_tests)
    set_tests_properties(category_summary_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(repository_tests tests/repository_tests.cpp)
    target_link_libraries(repository_tests PRIVATE kura_core Qt6::Test)
    add_test(NAME repository_tests COMMAND repository_tests)

    add_executable(window_tests tests/window_tests.cpp)
    target_link_libraries(window_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME window_tests COMMAND window_tests)
    set_tests_properties(window_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(summary_tests tests/summary_tests.cpp)
    target_link_libraries(summary_tests PRIVATE kura_core Qt6::Test)
    add_test(NAME summary_tests COMMAND summary_tests)

    add_executable(budget_tests tests/budget_tests.cpp)
    target_link_libraries(budget_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME budget_tests COMMAND budget_tests)
    set_tests_properties(budget_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(statistics_tests tests/statistics_tests.cpp)
    target_link_libraries(statistics_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME statistics_tests COMMAND statistics_tests)
    set_tests_properties(statistics_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(wishlist_tests tests/wishlist_tests.cpp)
    target_link_libraries(wishlist_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME wishlist_tests COMMAND wishlist_tests)
    set_tests_properties(wishlist_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(data_management_tests tests/data_management_tests.cpp)
    target_link_libraries(data_management_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME data_management_tests COMMAND data_management_tests)
    set_tests_properties(data_management_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(release_tests tests/release_tests.cpp)
    target_link_libraries(release_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME release_tests COMMAND release_tests)
    set_tests_properties(release_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
