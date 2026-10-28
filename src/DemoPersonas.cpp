#include "DemoPersonas.h"
#include "BudgetRepository.h"
#include "CategoryCatalog.h"
#include "ExecutionRepository.h"
#include "MonthlyReviewRepository.h"
#include "MonthlyStoryTemplate.h"
#include "PricePerspective.h"
#include "PriceRepository.h"
#include "ProductRepository.h"
#include "ShoppingProfileRepository.h"
#include "SupplementSchema.h"
#include "TransactionRepository.h"
#include "UnifiedWishlistRepository.h"
#include "WishlistRepository.h"
#include "WorkflowStorage.h"
#include <QBuffer>
#include <QFont>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QSqlDriver>
#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <array>

namespace {
using Persona = DemoPersonas::Persona;
QString s(const char *text) { return QString::fromUtf8(text); }
QDateTime at(const QDate &day, const QTime &time = QTime(15, 20)) {
    return QDateTime(day, time, QTimeZone::systemTimeZone()).toUTC();
}
QByteArray productCard(const QString &brand, const QString &name, qint64 cents) {
    QImage image(1000,600,QImage::Format_RGB32); image.fill(QColor("#F7F3EB"));
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(QColor("#E8DED0")); p.drawRoundedRect(QRectF(30,35,300,500),24,24);
    QPainterPath garment;
    garment.moveTo(122,175); garment.lineTo(76,214); garment.lineTo(106,270); garment.lineTo(127,255);
    garment.lineTo(124,397); garment.lineTo(242,397); garment.lineTo(239,255); garment.lineTo(260,270);
    garment.lineTo(290,214); garment.lineTo(244,175); garment.lineTo(211,162);
    garment.quadTo(183,201,155,162); garment.closeSubpath();
    p.setBrush(QColor("#A5B0A0"));
    if (name.contains(QStringLiteral("收纳"))) {
        p.drawRoundedRect(QRectF(80,240,200,140),10,10);
        p.setBrush(QColor("#C5CFBF")); p.drawRoundedRect(QRectF(72,226,216,28),6,6);
    } else p.drawPath(garment);
    p.setPen(QColor("#294438")); QFont font("Microsoft YaHei"); font.setPixelSize(30); font.setBold(true); p.setFont(font);
    p.drawText(QRect(365,60,600,50),Qt::AlignLeft,brand);
    font.setPixelSize(28); font.setBold(false); p.setFont(font);
    p.drawText(QRect(365,135,585,110),Qt::TextWordWrap,name);
    font.setPixelSize(42); p.setFont(font);
    p.drawText(QRect(365,270,585,75),Qt::AlignLeft,Transaction::formatAmount(cents));
    font.setPixelSize(22); p.setFont(font); p.setPen(QColor("#696A60"));
    p.drawText(QRect(365,375,585,140),Qt::TextWordWrap,s("课堂演示素材\n商品示意、价格和规格均为虚构\n不是品牌在售页面或报价")); p.end();
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG"); return bytes;
}

struct Clothing { const char *name; int cents; const char *brand; };
const std::array<std::array<Clothing,3>,6> studentWardrobe{{
    {{{"优衣库 U 圆领T恤",7900,"UNIQLO"},{"MUJI 棉麻衬衫",14900,"MUJI"},{"优衣库棉质短裤",9900,"UNIQLO"}}},
    {{{"MUJI 条纹短袖",7900,"MUJI"},{"优衣库百褶裙",12900,"UNIQLO"},{"MUJI 棉质长裤",14900,"MUJI"}}},
    {{{"优衣库连衣裙",14900,"UNIQLO"},{"MUJI 凉感衬衫",9900,"MUJI"},{"优衣库防晒外套",7900,"UNIQLO"}}},
    {{{"优衣库 AIRism 上衣",7900,"UNIQLO"},{"MUJI 棉质T恤",5900,"MUJI"},{"",0,""}}},
    {{{"优衣库棉质长裤",12900,"UNIQLO"},{"MUJI 针织背心",7900,"MUJI"},{"优衣库印花T恤",5900,"UNIQLO"}}},
    {{{"优衣库针织开衫",14900,"UNIQLO"},{"MUJI 牛津纺衬衫",12900,"MUJI"},{"优衣库直筒牛仔裤",19900,"UNIQLO"}}}
}};
const std::array<std::array<Clothing,3>,7> workerWardrobe{{
    {{{"COS 棉质通勤衬衫",45000,"COS"},{"ZARA 直筒西裤",25900,"ZARA"},{"ZARA 针织背心",15900,"ZARA"}}},
    {{{"COS 宽松棉质衬衫",35000,"COS"},{"ZARA 中长半裙",25900,"ZARA"},{"",0,""}}},
    {{{"COS 亚麻混纺长裤",55000,"COS"},{"ZARA 短袖衬衫",19900,"ZARA"},{"ZARA 基础T恤",9900,"ZARA"}}},
    {{{"COS 夏季连衣裙",69000,"COS"},{"ZARA 凉鞋",23900,"ZARA"},{"",0,""}}},
    {{{"COS 罗纹针织上衣",39000,"COS"},{"ZARA 轻薄长裤",22900,"ZARA"},{"ZARA 短袖上衣",15900,"ZARA"}}},
    {{{"COS 羊毛混纺马甲",59000,"COS"},{"ZARA 通勤衬衫",29900,"ZARA"},{"",0,""}}},
    {{{"COS 棉质长袖上衣",39000,"COS"},{"ZARA 直筒牛仔裤",29900,"ZARA"},{"",0,""}}}
}};

class Seeder {
public:
    Seeder(DatabaseManager &database, Persona persona, QString &error, const QDateTime &now)
        : db(database), worker(persona==Persona::Worker), error(error), now(now),
          today(now.toLocalTime().date()), month(today.year(),today.month(),1),
          expenses(database), budgets(database), monthly(database), products(database), wishes(database), catalog(database) {}
    bool run();
private:
    bool sql(const QString &statement,const QVariantList &values={}) {
        QSqlQuery query(db.database());
        if (!query.prepare(statement)) { error=query.lastError().text(); return false; }
        for (const auto &value:values) query.addBindValue(value);
        if (!query.exec()) { error=query.lastError().text(); return false; }
        return true;
    }
    qint64 add(const QString &name,qint64 cents,const QDate &date,const QString &detail,
               const QString &purpose,const QString &scope,const QString &note={},const QTime &time=QTime(12,20)) {
        Transaction record(0,name,cents,CategoryCatalog::legacyCategory(detail),date,time,
            s("虚构演示。")+note,detail,{},purpose,scope);
        if (!expenses.add(record)) { error=expenses.lastError(); return 0; }
        lastId=record.id();
        return record.id();
    }
    bool tag(qint64 id,int value,const QString &note) {
        if (!id) return false;
        if (!monthly.setTag(id,value,note)) { error=monthly.lastError(); return false; }
        return true;
    }
    bool product(const QString &kind,qint64 id,const QString &brand,const QString &reason,const QString &alternative) {
        ProductContext context; ProductProfile profile;
        if (!products.context(kind,id,context) || !products.ensure(context,profile)) { error=products.lastError(); return false; }
        profile.fields={{"brand",brand},{"model",s("DEMO-虚构款")},{"materials",s("演示设定：棉质；实际成分需核对商品标签")},
            {"specification",s("M码／试穿后再确认")},{"season",s("春秋，可叠穿")},
            {"features",s("优先考虑与已有衣物搭配，不因折扣增加重复款")},
            {"care",s("按衣物标签洗护，购买前确认退换政策")},{"purchase_reason",reason},{"alternatives",alternative}};
        if (context.name.contains(s("羊毛"))) profile.fields["materials"]=s("演示设定：羊毛混纺，具体比例待核对标签。");
        if (context.name.contains(s("收纳"))) {
            profile.fields["materials"]=s("演示设定：PP塑料，非真实商品规格。");
            profile.fields["specification"]=s("演示尺寸：18×12×9厘米；购买前实测桌面空间。");
            profile.fields["season"]=s("全年适用"); profile.fields["features"]=s("放文具与充电线；现有纸盒也是替代方案。");
            profile.fields["care"]=s("按标签清洁，避免接近热源。");
        }
        profile.notes=s("预置研究笔记（虚构）：先盘点衣柜，再比较面料、试穿和使用频率。所有价格均为演示设定，不代表品牌实时报价。");
        if (!products.save(profile,productCard(brand,context.name,context.amountCents),{{"demo",true},{"fields",profile.fields}})) { error=products.lastError(); return false; }
        return true;
    }
    bool spendingMonth(const QDate &first,int index,int lastDay);
    bool lists();
    bool savings();
    bool sessions();
    bool stories();
    qint64 wish(const QString &name,qint64 cents,const QString &detail,const QString &reason,int days,int age,int priority,
                const QString &brand={},const QString &alternative={}) {
        WishItem item; item.name=name; item.estimatedCents=cents; item.category=CategoryCatalog::legacyCategory(detail);
        item.detailCategory=detail; item.reason=reason; item.coolingDays=days; item.priority=priority;
        if (!wishes.add(item,now.addDays(-age))) { error=wishes.lastError(); return 0; }
        if (!brand.isEmpty() && !product("wish",item.id,brand,reason,alternative)) return 0;
        return item.id;
    }
    bool price(qint64 wishId,const QList<qint64> &amounts,qint64 target) {
        qint64 track=0;
        for (int i=0;i<amounts.size();++i) {
            const auto token=QStringLiteral("demo-price-%1-%2").arg(wishId).arg(i);
            if (!catalog.recordPrice(wishId,amounts[i],s("演示手动记价，非实时报价"),token,now.addDays(-int(amounts.size()-i-1)*4),track)) { error=catalog.lastError(); return false; }
        }
        PriceRepository prices(db);
        if (!prices.configure(track,target,QStringLiteral("https://example.com/demo/%1").arg(wishId))) { error=prices.lastError(); return false; }
        return true;
    }
    DatabaseManager &db; bool worker; QString &error; QDateTime now; QDate today,month;
    TransactionRepository expenses; BudgetRepository budgets; MonthlyReviewRepository monthly;
    ProductRepository products; WishlistRepository wishes; UnifiedWishlistRepository catalog;
    qint64 lastId=0,goalId=0; QList<qint64> order;
    QList<Transaction> clothingHistory;
};

bool Seeder::spendingMonth(const QDate &first,int index,int lastDay) {
    const bool summer=!worker && (first.month()==7 || first.month()==8);
    const bool travelMonth=worker?(index==1 || index==4):(index==1 || index==3);
    const int earliest=!worker && index==1?1:8;
    const int tripStart=earliest+(6-QDate(first.year(),first.month(),earliest).dayOfWeek()+7)%7;
    for (int day=1;day<=lastDay;++day) {
        const QDate date(first.year(),first.month(),day);
        const int n=(day*7+first.month()*3)%9;
        // Travel replaces that weekend's routine meals/commute, rather than
        // charging campus or home meals in a different city on the same day.
        if (travelMonth && (day==tripStart || day==tripStart+1)) continue;
        if (worker) {
            const bool weekday=date.dayOfWeek()<=5;
            if (weekday) {
                if (!add(s("早餐店豆浆与饭团"),750+n*50,date,"canteen","food","basic",{},QTime(8,10+n))) return false;
                if (!add(s("工作日午餐简餐"),2200+n*100,date,"canteen","food","basic",{},QTime(12,5+n*3))) return false;
                if (!add(day%3?s("下班买菜做晚饭"):s("加班简餐"),day%3?1550+n*100:2400+n*100,date,"canteen","food","basic",{},QTime(19,10+n))) return false;
                if (!add(s("地铁通勤往返"),600+(day%3)*100,date,"transport","travel","basic",{},QTime(8,35))) return false;
                if (day%3==0 && !add(s("午后咖啡"),1800+(n%4)*300,date,"daily","food","discretionary",s("已有办公室咖啡，今天为口味付费。"),QTime(14,30))) return false;
            } else {
                if (!add(s("周末超市买菜"),4200+n*150,date,"daily_supplies","food","basic",s("自己做饭的食材。"),QTime(11,15))) return false;
                if (day%2==0 && !add(s("朋友聚餐个人份额"),8800+n*300,date,"daily","food","discretionary",{},QTime(18,50))) return false;
            }
        } else {
            // Summer at home includes family-provided meals that are not personal expenses.
            if (summer && day%3!=0) continue;
            if (!add(summer?s("暑期早餐"):s("校园食堂早餐"),350+(n%5)*50,date,"canteen","food","basic",{},QTime(7,25+n*2))) return false;
            if (!add(summer?s("暑期自习午餐"):s("校园食堂午餐"),1050+(n%7)*50,date,"canteen","food","basic",{},QTime(12,10+n))) return false;
            if (day!=11 && day!=24 && !add(summer?s("暑期自习晚餐"):s("校园食堂晚餐"),1000+(n%6)*50,date,"canteen","food","basic",{},QTime(18,5+n*3))) return false;
        }
    }
    const auto once=[&](int day,const char *name,int cents,const char *detail,const char *purpose,const char *scope,const char *note="") {
        return day>lastDay || add(s(name),cents,QDate(first.year(),first.month(),day),s(detail),s(purpose),s(scope),s(note));
    };
    if (worker) {
        if (!once(2,"合租房房租",260000,"daily_supplies","other","basic","独立房间房租；工资之外不另造收入。")
            || !once(3,"水电燃气个人分摊",13680+index*390,"daily_supplies","other","basic")
            || !once(5,"手机与网络套餐",6900,"subscription","other","basic")
            || !once(8,"纸巾与洗护补货",5980+index*110,"daily_supplies","other","basic")
            || !once(12,"洗衣与衣物护理",3600,"daily_supplies","other","basic")
            || !once(16,"音乐月度订阅",1500,"subscription","fun","discretionary")
            || !once(18,"瑜伽团课单次",7900,"entertainment","fun","discretionary")
            || !once(23,"通勤护肤补货",15900,"daily_supplies","other","discretionary")
            || !once(26,"职业技能电子书",5900,"study","other","discretionary")) return false;
    } else {
        if (!once(3,"手机基础套餐",2900,"subscription","other","basic")
            || !once(5,"校园公交往返",400,"transport","travel","basic")
            || !once(13,"地铁与公交出行",800,"transport","travel","basic")
            || !once(7,"纸巾与牙膏补货",2480+index*80,"daily_supplies","other","basic")
            || !once(17,"宿舍洗衣",1200,"daily_supplies","other","basic")
            || !once(2,"课后奶茶",1200,"daily","food","discretionary")
            || !once(9,"自习咖啡",1500,"daily","food","discretionary")
            || !once(18,"水果酸奶加餐",1680,"daily","food","discretionary")
            || !once(27,"周末甜品",1390,"daily","food","discretionary")
            || !once(11,"同学聚餐个人份额",4800+index*300,"daily","food","discretionary")
            || !once(24,"朋友生日聚餐",5900,"daily","food","discretionary")
            || !once(15,"选读教辅与打印",3280,"study","other","discretionary")
            || !once(21,"音乐月度订阅",800,"subscription","fun","discretionary")
            || !once(25,"校园文创明信片",1290,"other","other","discretionary")) return false;
    }
    if (lastDay>=20) {
        const auto movie=add(s("周末电影票"),worker?4590:2990,QDate(first.year(),first.month(),20),"entertainment","fun","discretionary",{},QTime(19,30));
        if (!tag(movie,index%3?-1:1,index%3?s("跟风选片，剧情不合口味；下次先看简介。"):s("与朋友一起看，放松了一晚。"))) return false;
    }
    const auto &wardrobe=worker?workerWardrobe[qMin(index,6)]:studentWardrobe[qMin(index,5)];
    for (int i=0;i<3;++i) {
        const auto &piece=wardrobe[i]; const int day=6+i*8;
        if (!piece.cents || day>lastDay) continue;
        const QDate date(first.year(),first.month(),day);
        const auto id=add(s(piece.name),piece.cents,date,"online_shopping","clothing","discretionary",s("折扣为虚构设定；本月第%1件服饰。").arg(i+1),QTime(16,25+i*7));
        if (!id) return false;
        clothingHistory.append(Transaction(id,s(piece.name),piece.cents,s("购物"),date,QTime(16,25+i*7),{},"online_shopping",{},"clothing","discretionary"));
        if (!tag(id,i==2 && index%2?-1:1,i==2 && index%2?s("衣柜已有近似款，购买后使用少；下月先查重。"):s("已多次搭配现有衣物，尺码合适且容易护理。"))) return false;
        if (i==0 && !product("transaction",id,s(piece.brand),s("替换旧衣或补齐日常搭配。"),s("先穿现有近似款；确有缺口再买。"))) return false;
    }
    if (!worker && (index==1 || index==3)) {
        if (!once(tripStart,"周边两日游往返车票",index==1?9800:16800,"transport","travel","discretionary","旅行使用前几个月生活费结余，必要生活预算不挪用。")
            || !once(tripStart,"青旅住宿个人份额",index==1?12800:15800,"other","other","discretionary")
            || !once(tripStart+1,"旅行餐饮",7800,"daily","food","discretionary")
            || !once(tripStart+1,"展馆与景区门票",4800,"entertainment","fun","discretionary")) return false;
    }
    if (worker && (index==1 || index==4)) {
        if (!once(tripStart,"周末旅行高铁往返",index==1?38600:46800,"transport","travel","discretionary","旅行月可使用过去储蓄，不增加虚构工资。")
            || !once(tripStart,"周末旅行住宿个人份额",index==1?39800:45600,"other","other","discretionary")
            || !once(tripStart+1,"旅行餐饮与地方小吃",18600,"daily","food","discretionary")
            || !once(tripStart+1,"博物馆与展览门票",8800,"entertainment","fun","discretionary")) return false;
    }
    if ((worker && index==3) || (!worker && index==5)) {
        if (!once(19,"换季常备药与门诊",worker?6800:2980,"daily_supplies","other","basic")) return false;
    }
    return true;
}

bool Seeder::lists() {
    const auto ready=wish(worker?s("ZARA 通勤西裤"):s("优衣库基础针织开衫"),worker?29900:19900,"online_shopping",
        worker?s("现有西裤膝部磨损，想补一条能配三件上衣的通勤款。"):s("教室空调冷，想买能配现有衬衫的开衫。"),3,12,1,worker?s("ZARA"):s("UNIQLO"),
        worker?s("继续穿现有深灰西裤，或先修改裤脚。"):s("先用现有卫衣；MUJI相近款249元作为备选。"));
    const auto cooling=wish(worker?s("COS 羊毛混纺针织衫"):s("MUJI 双面绒衬衫"),worker?79000:24900,"online_shopping",
        s("喜欢触感与廓形，但先等冷静期结束，再核对衣柜重复度。"),7,0,2,worker?s("COS"):s("MUJI"),s("现有同色衣物；不买也是可选方案。"));
    const auto bargain=wish(worker?s("COS 羊毛混纺开衫"):s("MUJI 桌面收纳盒组合"),worker?59000:7900,worker?"online_shopping":"daily_supplies",
        worker?s("秋冬通勤叠穿，只有试穿合适且价格到目标才考虑。"):s("整理宿舍书桌，只买放得下的一组。"),3,16,2,worker?s("COS"):s("MUJI"),s("优先整理已有物品，确认尺寸后再买。"));
    goalId=wish(worker?s("轻便登机旅行箱"):s("学习用头戴式降噪耳机"),worker?89900:39900,"online_shopping",
        worker?s("短途旅行换轻一些的箱子，旧箱仍可继续用。"):s("图书馆与宿舍学习使用，先比较佩戴舒适度。"),5,20,1);
    const auto trip=wish(worker?s("年末三日旅行计划"):s("周末城市漫游计划"),worker?220000:60000,"transport",
        s("先安排日期和可用结余，不能挪用必要生活费。"),7,10,3);
    if (!ready || !cooling || !bargain || !goalId || !trip) return false;
    order={goalId,ready,cooling,bargain,trip};
    if (!price(bargain,worker?QList<qint64>{89000,79000,69000,59000}:QList<qint64>{9900,8900,7900},worker?59000:7900)
        || !price(cooling,worker?QList<qint64>{79000}:QList<qint64>{24900},worker?69000:19900)
        || !price(goalId,worker?QList<qint64>{109900,99900,89900}:QList<qint64>{49900,44900,39900},worker?79900:34900)) return false;
    // Connect bought wishes to existing historical expenses rather than creating a second expense.
    for (int i=0;i<2;++i) {
        const auto &record=clothingHistory[clothingHistory.size()-4+i];
        const auto created=at(record.date().addDays(-8));
        WishItem item; item.name=record.description(); item.estimatedCents=record.amountCents()+2000;
        item.category=record.category(); item.detailCategory=record.detailCategory(); item.reason=s("先试穿并等待冷静期，实际在折扣时购入。"); item.coolingDays=3;
        if (!wishes.add(item,created)) { error=wishes.lastError(); return false; }
        if (!sql("UPDATE wishlist SET status=1,closed_ms=?,transaction_id=? WHERE id=?",{at(record.date(),record.time()).toMSecsSinceEpoch(),record.id(),item.id})) return false;
        if (!products.carry("transaction",record.id(),"wish",item.id)) { error=products.lastError(); return false; }
    }
    const QStringList rejected=worker?QStringList{s("同款第四件白衬衫"),s("限量香氛礼盒")}:QStringList{s("跟风联名卫衣"),s("桌面氛围灯"),s("同色系帆布包")};
    for (int i=0;i<rejected.size();++i) {
        const auto id=wish(rejected[i],worker?69900:12900+i*5000,"online_shopping",s("想买时先放清单，冷静后发现现有物品已经足够。"),3,18+i,3);
        if (!id || !sql("UPDATE wishlist SET status=2,closed_ms=? WHERE id=?",{now.addDays(-3-i).toMSecsSinceEpoch(),id})) return false;
    }
    if (!catalog.saveOrder(order)) { error=catalog.lastError(); return false; }
    return true;
}

bool Seeder::savings() {
    // These are resisted-purchase records, not income or cash transfers.
    QSqlQuery pool(db.database());
    pool.prepare("INSERT INTO wish_pools(wish_id,name,target_cents,created_ms) SELECT id,name,estimated_cents,? FROM wishlist WHERE id=?");
    pool.addBindValue(now.addDays(-20).toMSecsSinceEpoch()); pool.addBindValue(goalId);
    if (!pool.exec()) { error=pool.lastError().text(); return false; }
    const auto poolId=pool.lastInsertId().toLongLong(); pool.finish();
    const QStringList names=worker?QStringList{s("放弃重复白衬衫"),s("取消随手凑单"),s("把一次打车换成地铁")}:QStringList{s("忍住限时奶茶券凑单"),s("放弃重复帆布包"),s("自带水少买饮料")};
    const QList<int> amounts=worker?QList<int>{29900,6900,5200}:QList<int>{1800,5900,1200};
    for (int i=0;i<3;++i) if (!sql("INSERT INTO savings_records(pool_id,amount_cents,description,created_ms,request_token) VALUES(?,?,?,?,?)",
        {poolId,amounts[i],names[i],now.addDays(-14+i*4).toMSecsSinceEpoch(),QStringLiteral("demo-current-resist-%1").arg(i)})) return false;
    for (int i=0;i<6;++i) {
        const auto date=month.addMonths(-6+i).addDays(20);
        if (!sql("INSERT INTO savings_records(pool_id,amount_cents,description,created_ms,request_token) VALUES(?,?,?,?,?)",
            {poolId,worker?5900+i*700:2500+i*300,worker?s("少买重复小物，留给更想要的东西"):s("忍住一次冲动小物"),at(date).toMSecsSinceEpoch(),QStringLiteral("demo-history-resist-%1").arg(i)})) return false;
    }
    return true;
}

bool Seeder::sessions() {
    const QStringList names=worker?QStringList{s("限量香氛礼盒"),s("旅行便携相机"),s("COS 棉质通勤上衣")}
                                  :QStringList{s("跟风联名卫衣"),s("学习降噪耳机对比"),s("MUJI 棉麻衬衫")};
    for (int i=0;i<3;++i) {
        const auto &bought=clothingHistory[clothingHistory.size()-3];
        const auto started=i==2?at(bought.date().addDays(-4)):now.addDays(-22+i*5);
        ExecutionSession session; session.name=i==2?bought.description():names[i]; session.category="online_shopping"; session.estimatedCents=i==2?bought.amountCents()+2000:worker?39900+i*10000:12900+i*5000;
        session.mode=i==1?"research":"fast"; session.durationSeconds=i==1?1800:300;
        ExecutionRepository executions(db);
        if (!executions.start(session,started)
            || !executions.saveNotes(session.id,s("本地预置研究笔记：列出使用场景、已有替代品和可用预算，先核对再决定。"),QStringLiteral(""))) { error=executions.lastError(); return false; }
        if (i==2) {
            if (!sql("UPDATE execution_sessions SET state='bought',closed_ms=?,transaction_id=? WHERE id=?",{at(bought.date(),bought.time()).toMSecsSinceEpoch(),bought.id(),session.id})) return false;
            if (!products.carry("transaction",bought.id(),"execution",session.id)) { error=products.lastError(); return false; }
        } else if (!executions.finish(session.id,i==0?"skipped":"undecided",started.addSecs(session.durationSeconds))) { error=executions.lastError(); return false; }
    }
    // No active session blocks the presenter from starting a fresh decision.
    return true;
}

bool Seeder::stories() {
    for (int i=-6;i<=0;++i) {
        const auto date=month.addMonths(i); MonthlyReviewData data;
        if (!monthly.load(date,data)) { error=monthly.lastError(); return false; }
        if (data.hasData()) {
            const auto story=MonthlyStoryTemplate::local(data);
            if (!monthly.saveStory(date,data.factsHash,MonthlyStoryTemplate::encode(story),MonthlyStoryTemplate::personaCaption(story),"local")) { error=monthly.lastError(); return false; }
        }
    }
    return true;
}

bool Seeder::run() {
    WorkflowStorage::Transaction write(db.database(),error); if (!write.ok()) return false;
    for (int i=-6;i<=0;++i) {
        const auto date=month.addMonths(i);
        if (!budgets.saveSplit(date,worker?520000:100000,worker?280000:100000)) { error=budgets.lastError(); return false; }
        if (i==0 && !worker) continue;
        const int days=i==0?qMax(0,today.day()-1):date.daysInMonth();
        if (!spendingMonth(date,i+6,days)) return false;
    }
    ShoppingProfileRepository profile(db);
    const QStringList rules=worker?QStringList{
        s("月到手10000元，先留2000元储蓄；日常消费预算8000元，必要生活5200元、自由消费2800元。"),
        s("通勤衣物先检查衣柜，至少能搭三套；COS与ZARA也不因为品牌或折扣跳过试穿。"),
        s("超过500元先放清单冷静7天；旅行额外开销只能使用以往结余。"),
        s("不凑单；比较面料、版型、洗护成本与退换条件，已有替代品时允许不买。")}
        :QStringList{s("月生活费2000元，必要生活与自由消费各1000元，不挪用吃饭钱。"),
        s("每月通常买2至3件衣服；MUJI和优衣库先试穿、看衣柜，再考虑折扣。"),
        s("超过200元先冷静3天；重复款先不买，旅行从往月生活费结余安排。"),
        s("喜欢不等于现在必须买；把预计使用次数和已有替代品写下来。")};
    if (!profile.saveRules(rules)) { error=profile.lastError(); return false; }
    if (!lists() || !savings() || !sessions() || !stories()) return false;
    if (!WorkflowSchema::validate(db.database(),error) || !ProductSchema::validate(db.database(),error)
        || !SupplementSchema::validate(db.database(),error)) return false;
    return write.commit();
}
}

DemoPersonas::Session::Session():m_directory(std::make_unique<QTemporaryDir>()) {}
DemoPersonas::Session::~Session()=default;
QString DemoPersonas::Session::open(Persona persona,QString &error,const QDateTime &now) {
    error.clear();
    if (!m_directory->isValid() || (persona!=Persona::Student && persona!=Persona::Worker)) { error=s("无法创建临时演示空间。"); return {}; }
    const auto name=key(persona); const auto file=m_directory->filePath(name+".sqlite");
    if (m_ready.contains(name)) {
        if (!QFile::exists(file)) { error=s("临时演示文件已被移除，请重新启动演示。"); return {}; }
        return file;
    }
    DatabaseManager database(file);
    if (!database.initialize()) { error=database.lastError(); return {}; }
    if (!populate(database,persona,error,now)) return {};
    m_ready.insert(name);
    return file;
}

QString DemoPersonas::key(Persona persona) { return persona==Persona::Worker?QStringLiteral("worker"):QStringLiteral("student"); }
QString DemoPersonas::title(Persona persona) { return persona==Persona::Worker?QStringLiteral("知夏 · 职场女生"):QStringLiteral("小满 · 女大学生"); }
QString DemoPersonas::description(Persona persona) {
    return persona==Persona::Worker
        ?s("月到手 10,000 元，日常预算 8,000 元，计划储蓄 2,000 元。喜欢 COS／ZARA，工作日通勤，周末聚会和旅行。")
        :s("月生活费 2,000 元，必要生活和自由消费各 1,000 元。喜欢 MUJI／优衣库，每月买两三件衣服，偶尔短途旅行；本月账单留待现场导入。");
}
bool DemoPersonas::populate(DatabaseManager &database,Persona persona,QString &error,const QDateTime &now) {
    error.clear();
    if ((persona!=Persona::Student && persona!=Persona::Worker) || !now.isValid() || !now.toLocalTime().date().addMonths(-6).isValid()
        || !database.database().isOpen()) { error=s("演示身份、时间或数据库不可用。"); return false; }
    // Include product-only, achievement-only and newly added tables in the no-overwrite guard.
    for (const auto &table:database.database().tables(QSql::Tables)) {
        if (table.startsWith("sqlite_")) continue;
        QSqlQuery query(database.database());
        const auto escaped=database.database().driver()->escapeIdentifier(table,QSqlDriver::TableName);
        if (!query.exec("SELECT 1 FROM "+escaped+" LIMIT 1")) { error=query.lastError().text(); return false; }
        if (query.next()) { error=s("演示数据只可写入空数据库；已有内容未改动。"); return false; }
    }
    Seeder seed(database,persona,error,now);
    if (!seed.run()) return false;
    if (database.databasePath()!=":memory:") {
        PricePerspective perspective; perspective.mode=persona==Persona::Worker?PricePerspective::Worker:PricePerspective::Student;
        perspective.dailyFoodCents=3000; perspective.mealsPerDay=3; perspective.monthlyIncomeCents=1000000;
        perspective.workDaysPerMonth=22; perspective.dailyWorkHours=8;
        if (!PricePerspectiveStore::save(PricePerspectiveStore::settingsPath(database.databasePath()),perspective,error)) return false;
        QSettings settings(PricePerspectiveStore::settingsPath(database.databasePath()),QSettings::IniFormat);
        settings.setValue("demo/persona",key(persona)); settings.sync();
        if (settings.status()!=QSettings::NoError) { error=s("演示偏好保存失败。"); return false; }
    }
    return true;
}
