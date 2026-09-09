#include "gps.h"
#include "ui_gps.h"
#include <QCoreApplication>
#include <QTextStream>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <QDebug>  // 新增：包含QDebug头文件
gps::gps(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::gps),
    m_originalWindow(parent)  // 保存原窗口指针（从构造函数传入）
{
    ui->setupUi(this);
    setFixedSize(800, 480);
    setWindowTitle("GPPL5000 · GPS 记录");
    QVBoxLayout *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(10, 8, 10, 8);
    pageLayout->setSpacing(8);
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *pageTitle = new QLabel("GPS 定位数据", this);
    pageTitle->setObjectName("panelTitle");
    ui->pushButton->setText("返回主界面");
    ui->pushButton->setProperty("role", "danger");
    ui->pushButton_2->setText("保存 GPS 记录");
    ui->pushButton_2->setProperty("role", "primary");
    headerLayout->addWidget(pageTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(ui->pushButton_2);
    headerLayout->addWidget(ui->pushButton);
    pageLayout->addLayout(headerLayout);
    ui->textBrowser->setObjectName("eventLog");
    pageLayout->addWidget(ui->textBrowser, 1);
    ui->textBrowser->setReadOnly(true);
    // 初始显示标题（明确格式）
    ui->textBrowser->append("📡 GPS数据接收日志");
    ui->textBrowser->append("==================================");
    ui->textBrowser->append("格式：序号（No.） | 纬度（8位小数） | 经度（8位小数）");
    ui->textBrowser->append("==================================");

}

gps::~gps()
{
    delete ui;
}

void gps::on_pushButton_clicked()
{
    this->close();  // 关闭当前GPS窗口
    if (m_originalWindow) {
        m_originalWindow->show();  // 显示原窗口
    }
}

void gps::onGpsDataReceived(double latitude, double longitude)
{
    // 1. 先打印日志（调试用，确认信号触发）
    qDebug() << "[GPS槽函数] 收到新数据：纬度=" << latitude << " 经度=" << longitude;

    // 2. 过滤无效数据（避免收到0.0等无效值时显示空数据）
    if (qFuzzyCompare(latitude, 0.0) && qFuzzyCompare(longitude, 0.0)) {
        qDebug() << "[GPS槽函数] 无效数据（经纬度均为0），跳过显示";
        return;
    }

    // 3. 构造当前组数据（仅经纬度，无时间）
    GpsData currentData;
    currentData.latitude = latitude;
    currentData.longitude = longitude;

    // 4. 缓存数据（追加不覆盖）
    m_allGpsData.append(currentData);

    // 5. 显示到界面（序号区分，优化格式）
    int dataIndex = m_allGpsData.size();
    // 序号补0+加粗，无时间戳
    QString indexStr = QString("<span style='font-weight: bold; color: #2c3e50;'>No.%1</span>")
                      .arg(dataIndex, 2, 10, QChar('0'));
    // 8位小数显示（确保精度）
    QString latStr = QString::number(latitude, 'f', 8);
    QString lonStr = QString::number(longitude, 'f', 8);
    // 组合显示文本（优化分隔符，更清晰）
    QString displayText = QString("%1 | 纬度：%2 | 经度：%3")
                          .arg(indexStr)
                          .arg(latStr)
                          .arg(lonStr);

    // 6. 追加到textBrowser（确保显示且滚动到底部）
    ui->textBrowser->append(displayText);
    ui->textBrowser->append("----------------------------------");
    ui->textBrowser->moveCursor(QTextCursor::End); // 强制滚动到最新数据
}

// 保存按钮逻辑（无任何时间相关内容）
void gps::on_pushButton_2_clicked()
{
    if (m_allGpsData.isEmpty()) {
        QMessageBox::warning(this, "⚠️ 警告", "暂无接收任何GPS数据！");
        return;
    }

    // 文件名：仅用GPS_所有数据.txt（无时间戳，如需区分可手动改）
    QString filePath = QCoreApplication::applicationDirPath() + "/GPS_所有数据.txt";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::critical(this, "❌ 错误", "文件打开失败！\n原因：" + file.errorString());
        return;
    }

    QTextStream out(&file);
    // 文件头（无时间相关）
    out << "GPS数据保存日志\n";
    out << "总组数：" << m_allGpsData.size() << "\n";
    out << "==================================\n";
    out << "序号\t纬度（8位小数）\t经度（8位小数）\n";
    out << "==================================\n";

    // 写入所有数据（仅序号+经纬度）
    for (int i = 0; i < m_allGpsData.size(); ++i) {
        const GpsData &data = m_allGpsData[i];
        out << (i+1) << "\t"
            << QString::number(data.latitude, 'f', 8) << "\t"
            << QString::number(data.longitude, 'f', 8) << "\n";
    }

    file.close();

    // 成功提示（无时间）
    QMessageBox::information(this, "✅ 成功",
        QString("所有GPS数据已保存！\n")
        + QString("📊 总组数：%1\n").arg(m_allGpsData.size())
        + QString("📁 保存路径：%2").arg(filePath));
}
