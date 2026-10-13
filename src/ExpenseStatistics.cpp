#include "ExpenseStatistics.h"
#include <limits>

qint64 YearlyExpenseStatistics::averageMonthCents() const
{
    return totalCents / 12 + (totalCents % 12 >= 6 ? 1 : 0);
}

int YearlyExpenseStatistics::peakMonthIndex() const
{
    int peak = -1;
    for (qsizetype i = 0; i < months.size(); ++i) {
        if (months[i].totalCents > 0 && (peak < 0 || months[i].totalCents > months[peak].totalCents))
            peak = static_cast<int>(i);
    }
    return peak;
}

bool ExpenseStatistics::calculateYear(const QList<Transaction> &records, int year,
                                     YearlyExpenseStatistics &result, QString &error)
{
    error.clear();
    if (year < 1 || year > 9999) {
        error = QStringLiteral("统计年份须在 1 至 9999 之间。");
        return false;
    }
    YearlyExpenseStatistics loaded;
    loaded.year = year;
    for (int month = 1; month <= 12; ++month)
        loaded.months.append({QDate(year, month, 1), 0, 0});
    for (const auto &record : records) {
        const QString validation = record.validationError();
        if (!validation.isEmpty()) {
            error = QStringLiteral("无法统计无效的消费记录：%1").arg(validation);
            return false;
        }
        if (record.date().year() != year)
            continue;
        if (record.amountCents() > std::numeric_limits<qint64>::max() - loaded.totalCents) {
            error = QStringLiteral("年度金额超出可计算范围。");
            return false;
        }
        auto &month = loaded.months[record.date().month() - 1];
        month.totalCents += record.amountCents();
        ++month.recordCount;
        loaded.totalCents += record.amountCents();
        ++loaded.recordCount;
    }
    result = loaded;
    return true;
}
