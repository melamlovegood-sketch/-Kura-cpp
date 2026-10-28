cmake_minimum_required(VERSION 3.16)
project(KuraCpp VERSION 0.12.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets Sql Network)
find_package(ZLIB REQUIRED)

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
    src/UnifiedWishlistRepository.cpp src/UnifiedWishlistRepository.h
    src/CsvExporter.cpp src/CsvExporter.h
    src/DatabaseBackupService.cpp src/DatabaseBackupService.h
    src/DemoData.cpp src/DemoData.h
    src/CategoryCatalog.cpp src/CategoryCatalog.h
    src/DecisionSchema.cpp src/DecisionSchema.h
    src/WishPoolRepository.cpp src/WishPoolRepository.h
    src/AISettings.cpp src/AISettings.h
    src/WorkflowStorage.h
    src/WorkflowSchema.cpp src/WorkflowSchema.h
    src/ShoppingProfileRepository.cpp src/ShoppingProfileRepository.h
    src/ExecutionRepository.cpp src/ExecutionRepository.h
    src/ReviewRepository.cpp src/ReviewRepository.h
    src/PurchaseRepository.cpp src/PurchaseRepository.h
    src/PriceRepository.cpp src/PriceRepository.h
    src/ProductRepository.cpp src/ProductRepository.h
    src/SupplementSchema.cpp src/SupplementSchema.h
    src/XlsxReader.cpp src/XlsxReader.h
    src/BillParser.cpp src/BillParser.h
    src/BillImportRepository.cpp src/BillImportRepository.h
    src/MonthlyReviewRepository.cpp src/MonthlyReviewRepository.h
    src/MonthlyStoryTemplate.cpp src/MonthlyStoryTemplate.h
    src/BillingAggregation.cpp src/BillingAggregation.h
)
target_include_directories(kura_core PUBLIC src)
target_link_libraries(kura_core PUBLIC Qt6::Sql Qt6::Gui ZLIB::ZLIB)
if(WIN32)
    target_link_libraries(kura_core PUBLIC crypt32)
endif()

add_library(kura_widgets STATIC
    resources/kura.qrc
    src/mainwindow.cpp src/mainwindow.h
    src/AddTransactionDialog.cpp src/AddTransactionDialog.h
    src/BudgetWidget.cpp src/BudgetWidget.h
    src/MonthlyTrendChart.cpp src/MonthlyTrendChart.h
    src/StatisticsWidget.cpp src/StatisticsWidget.h
    src/WishDialog.cpp src/WishDialog.h
    src/WishlistWidget.cpp src/WishlistWidget.h
    src/WishPriceDialog.cpp src/WishPriceDialog.h
    src/DataManagementWidget.cpp src/DataManagementWidget.h
    src/AppTheme.cpp src/AppTheme.h
    src/DialogUi.cpp src/DialogUi.h
    src/AppTypography.cpp src/AppTypography.h
    src/HomeWidget.cpp src/HomeWidget.h
    src/AppearanceWidget.cpp src/AppearanceWidget.h
    src/ShellWidgets.h
    src/ProductList.h
    src/AIService.cpp src/AIService.h
    src/DecisionDialog.cpp src/DecisionDialog.h
    src/WishPoolWidget.cpp src/WishPoolWidget.h
    src/ImagePreview.h
    src/ShoppingProfileDialog.cpp src/ShoppingProfileDialog.h
    src/ExecutionWidget.cpp src/ExecutionWidget.h
    src/DecisionChatWidget.cpp src/DecisionChatWidget.h
    src/ReviewWidget.cpp src/ReviewWidget.h
    src/PriceWidget.cpp src/PriceWidget.h
    src/PriceRecordDialog.cpp src/PriceRecordDialog.h
    src/ProductDetailsDialog.cpp src/ProductDetailsDialog.h
    src/WorkflowOverview.cpp src/WorkflowOverview.h
    src/BillImportDialog.cpp src/BillImportDialog.h
    src/MonthlyReviewWidget.cpp src/MonthlyReviewWidget.h
    src/BillingWorkspace.cpp src/BillingWorkspace.h
    src/BillingCharts.cpp src/BillingCharts.h
    src/ExpenseComposer.cpp src/ExpenseComposer.h
    src/ModernSelectors.cpp src/ModernSelectors.h
)
target_link_libraries(kura_widgets PUBLIC kura_core Qt6::Widgets Qt6::Network)
add_executable(KuraCpp WIN32 src/main.cpp)
target_link_libraries(KuraCpp PRIVATE kura_widgets)
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(secondary_ui_tests tests/secondary_ui_tests.cpp)
    target_link_libraries(secondary_ui_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME secondary_ui_tests COMMAND secondary_ui_tests)
    set_tests_properties(secondary_ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(product_details_tests tests/product_details_tests.cpp)
    target_link_libraries(product_details_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME product_details_tests COMMAND product_details_tests)
    set_tests_properties(product_details_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(decision_chat_tests tests/decision_chat_tests.cpp)
    target_link_libraries(decision_chat_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME decision_chat_tests COMMAND decision_chat_tests)
    set_tests_properties(decision_chat_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(dialog_tests tests/dialog_tests.cpp)
    target_link_libraries(dialog_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME dialog_tests COMMAND dialog_tests)
    set_tests_properties(dialog_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(price_record_tests tests/price_record_tests.cpp)
    target_link_libraries(price_record_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME price_record_tests COMMAND price_record_tests)
    set_tests_properties(price_record_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(typography_tests tests/typography_tests.cpp)
    target_link_libraries(typography_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME typography_tests COMMAND typography_tests)
    set_tests_properties(typography_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(billing_tests tests/billing_tests.cpp)
    target_link_libraries(billing_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME billing_tests COMMAND billing_tests)
    set_tests_properties(billing_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(planning_layout_tests tests/planning_layout_tests.cpp)
    target_link_libraries(planning_layout_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME planning_layout_tests COMMAND planning_layout_tests)
    set_tests_properties(planning_layout_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(execution_ui_tests tests/execution_ui_tests.cpp)
    target_link_libraries(execution_ui_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME execution_ui_tests COMMAND execution_ui_tests)
    set_tests_properties(execution_ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(review_layout_tests tests/review_layout_tests.cpp)
    target_link_libraries(review_layout_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME review_layout_tests COMMAND review_layout_tests)
    set_tests_properties(review_layout_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(import_tests tests/import_tests.cpp)
    target_link_libraries(import_tests PRIVATE kura_core Qt6::Test)
    add_test(NAME import_tests COMMAND import_tests)
    add_executable(monthly_review_tests tests/monthly_review_tests.cpp)
    target_link_libraries(monthly_review_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME monthly_review_tests COMMAND monthly_review_tests)
    add_executable(supplement_ui_tests tests/supplement_ui_tests.cpp)
    target_link_libraries(supplement_ui_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME supplement_ui_tests COMMAND supplement_ui_tests)
    set_tests_properties(supplement_ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(workflow_tests tests/workflow_tests.cpp)
    target_link_libraries(workflow_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME workflow_tests COMMAND workflow_tests)
    set_tests_properties(workflow_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(decision_tests tests/decision_tests.cpp)
    target_link_libraries(decision_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME decision_tests COMMAND decision_tests)
    set_tests_properties(decision_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(shell_tests tests/shell_tests.cpp)
    target_link_libraries(shell_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME shell_tests COMMAND shell_tests)
    set_tests_properties(shell_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
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
