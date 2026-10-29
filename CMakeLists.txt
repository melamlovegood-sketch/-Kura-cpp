cmake_minimum_required(VERSION 3.16)
project(KuraCpp VERSION 0.17.1 LANGUAGES CXX)
string(TIMESTAMP KURA_BUILD_DATE "%Y-%m-%d")
configure_file(src/BuildInfo.h.in generated/BuildInfo.h @ONLY)

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
    src/AchievementRepository.cpp src/AchievementRepository.h
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
    src/AutoBackupService.cpp src/AutoBackupService.h
    src/DemoData.cpp src/DemoData.h
    src/DemoPersonas.cpp src/DemoPersonas.h
    src/CategoryCatalog.cpp src/CategoryCatalog.h
    src/CategoryRepository.cpp src/CategoryRepository.h
    src/PricePerspective.cpp src/PricePerspective.h
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
target_include_directories(kura_core PUBLIC src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(kura_core PUBLIC Qt6::Sql Qt6::Gui ZLIB::ZLIB)
if(WIN32)
    target_link_libraries(kura_core PUBLIC crypt32)
endif()

add_library(kura_widgets STATIC
    src/CategoryWidgets.cpp src/CategoryWidgets.h
    src/AchievementCelebration.cpp src/AchievementCelebration.h src/AchievementArt.h
    src/EmptyAmountSpinBox.h
    src/BatchSelectionBar.h
    src/DemoMode.cpp src/DemoMode.h
    src/AchievementWall.cpp src/AchievementWall.h
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
    src/SplashScreen.cpp src/SplashScreen.h
    src/OnboardingDialog.cpp src/OnboardingDialog.h
    src/OnboardingTour.cpp src/OnboardingTour.h src/KuraMascot.h
    src/HomeWidget.cpp src/HomeWidget.h
    src/AppearanceWidget.cpp src/AppearanceWidget.h
    src/PricePerspectiveWidget.cpp src/PricePerspectiveWidget.h
    src/PricePerspectiveHint.cpp src/PricePerspectiveHint.h
    src/ShellWidgets.h
    src/ScreenshotDropTarget.h
    src/ProductList.h
    src/AIService.cpp src/AIService.h
    src/DecisionDialog.cpp src/DecisionDialog.h
    src/WishPoolWidget.cpp src/WishPoolWidget.h
    src/ImagePreview.h
    src/ExecutionWidget.cpp src/ExecutionWidget.h
    src/DecisionChatWidget.cpp src/DecisionChatWidget.h
    src/ReviewWidget.cpp src/ReviewWidget.h
    src/PriceWidget.cpp src/PriceWidget.h
    src/PriceRecordDialog.cpp src/PriceRecordDialog.h
    src/ProductDetailsDialog.cpp src/ProductDetailsDialog.h
    src/ProductShareDialog.cpp src/ProductShareDialog.h
    src/WorkflowOverview.cpp src/WorkflowOverview.h
    src/BillImportDialog.cpp src/BillImportDialog.h
    src/MonthlyReviewWidget.cpp src/MonthlyReviewWidget.h
    src/BillingWorkspace.cpp src/BillingWorkspace.h
    src/BillingCharts.cpp src/BillingCharts.h
    src/ExpenseComposer.cpp src/ExpenseComposer.h
    src/ExpenseClassification.cpp src/ExpenseClassification.h
    src/ModernSelectors.cpp src/ModernSelectors.h
)
target_link_libraries(kura_widgets PUBLIC kura_core Qt6::Widgets Qt6::Network)
add_executable(KuraCpp WIN32 src/main.cpp)
if(WIN32)
    enable_language(RC)
    target_sources(KuraCpp PRIVATE resources/kura.rc)
    target_include_directories(KuraCpp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons")
endif()
target_link_libraries(KuraCpp PRIVATE kura_widgets)
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(batch_delete_tests tests/batch_delete_tests.cpp)
    target_link_libraries(batch_delete_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME batch_delete_tests COMMAND batch_delete_tests)
    set_tests_properties(batch_delete_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(category_management_tests tests/category_management_tests.cpp)
    target_link_libraries(category_management_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME category_management_tests COMMAND category_management_tests)
    set_tests_properties(category_management_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(onboarding_tests tests/onboarding_tests.cpp)
    target_link_libraries(onboarding_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME onboarding_tests COMMAND onboarding_tests)
    set_tests_properties(onboarding_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(trial_ai_tests tests/trial_ai_tests.cpp)
    target_link_libraries(trial_ai_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME trial_ai_tests COMMAND trial_ai_tests)
    set_tests_properties(trial_ai_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(settings_tests tests/settings_tests.cpp)
    target_link_libraries(settings_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME settings_tests COMMAND settings_tests)
    set_tests_properties(settings_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(achievement_tests tests/achievement_tests.cpp)
    target_link_libraries(achievement_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME achievement_tests COMMAND achievement_tests)
    set_tests_properties(achievement_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(price_perspective_tests tests/price_perspective_tests.cpp)
    target_link_libraries(price_perspective_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME price_perspective_tests COMMAND price_perspective_tests)
    set_tests_properties(price_perspective_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(splash_tests tests/splash_tests.cpp)
    target_link_libraries(splash_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME splash_tests COMMAND splash_tests)
    set_tests_properties(splash_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(expense_classification_tests tests/expense_classification_tests.cpp)
    target_link_libraries(expense_classification_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME expense_classification_tests COMMAND expense_classification_tests)
    set_tests_properties(expense_classification_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(screenshot_drop_tests tests/screenshot_drop_tests.cpp)
    target_link_libraries(screenshot_drop_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME screenshot_drop_tests COMMAND screenshot_drop_tests)
    set_tests_properties(screenshot_drop_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(product_share_tests tests/product_share_tests.cpp)
    target_link_libraries(product_share_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME product_share_tests COMMAND product_share_tests)
    set_tests_properties(product_share_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
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

    add_executable(demo_personas_tests tests/demo_personas_tests.cpp)
    target_link_libraries(demo_personas_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME demo_personas_tests COMMAND demo_personas_tests)
    set_tests_properties(demo_personas_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(release_tests tests/release_tests.cpp)
    target_link_libraries(release_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME release_tests COMMAND release_tests)
    set_tests_properties(release_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(amount_input_tests tests/amount_input_tests.cpp)
    target_link_libraries(amount_input_tests PRIVATE kura_widgets Qt6::Test)
    add_test(NAME amount_input_tests COMMAND amount_input_tests)
    set_tests_properties(amount_input_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
