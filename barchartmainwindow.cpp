//#include "barchartmainwindow.h"
//#include "ui_barchartmainwindow.h"
//#include <QDebug>
//#include <cmath>
//#include <vector>
//#include <algorithm>

//// 模拟数据（按顺序使用：18, 10, 14, 19, 15, 8, 7.2）
//const std::vector<double> SIMULATED_DATA = {18.0, 10.0, 14.0, 19.0, 15.0, 8.0, 7.2};

////1原始算法
//// 残差计算：动态调整峰值约束权重
//std::vector<double> BarChartMainWindow::computeResiduals(const double* params, const std::vector<double>& V, double a_val)
//{
//    double k0 = params[0];
//    double k2 = params[1];
//    double h = params[2];
//    double x0 = params[3];
//    std::vector<double> residuals;

//    // 1. 数据残差（7个检测点）
//    for (int i = 0; i < 7; ++i) {
//        double x = a_val * i;
//        double distance = sqrt(h * h + (x + x0) * (x + x0));
//        double theory = (k0 * k2 * exp(-distance)) / distance;
//        residuals.push_back(V[i] - theory);
//    }

//    // 2. 动态计算峰值约束权重（根据数据是否有特征调整）
//    double dataVariance = 0.0; // 数据方差（判断是否有特征）
//    double meanV = 0.0;
//    for (double v : V) meanV += v;
//    meanV /= V.size();
//    for (double v : V) dataVariance += (v - meanV) * (v - meanV);
//    dataVariance /= V.size();

//    // 数据方差小时（如全零），降低峰值约束权重（最小1e-2）
//    double peakWeight = std::max(1e-2, dataVariance * 1e3);

//    // 峰值位置约束
//    std::vector<double> theory_values;
//    std::vector<double> x_offsets;
//    for (int i = 0; i < 7; ++i) {
//        double x = a_val * i;
//        x_offsets.push_back(x);
//        double distance = sqrt(h * h + (x + x0) * (x + x0));
//        theory_values.push_back((k0 * k2 * exp(-distance)) / distance);
//    }

//    int peak_idx = 0;
//    double max_val = theory_values[0];
//    for (int i = 1; i < 7; ++i) {
//        if (theory_values[i] > max_val) {
//            max_val = theory_values[i];
//            peak_idx = i;
//        }
//    }
//    double peak_offset = x_offsets[peak_idx];
//    double target_offset = 3 * a_val;
//    residuals.push_back(peakWeight * (peak_offset - target_offset)); // 动态权重

//    return residuals;
//}

//// 计算目标函数值（不变）
//double BarChartMainWindow::computeObjective(const double* params, const std::vector<double>& V, double a_val)
//{
//    std::vector<double> residuals = computeResiduals(params, V, a_val);
//    double sum = 0.0;
//    for (double r : residuals) sum += r * r;
//    return 0.5 * sum;
//}

//// 数值法计算雅克比矩阵（不变）
//std::vector<std::vector<double>> BarChartMainWindow::computeJacobian(const double* params, const std::vector<double>& V, double a_val)
//{
//    const double epsilon = 1e-8;
//    int num_params = 4;
//    std::vector<double> r0 = computeResiduals(params, V, a_val);
//    int num_residuals = r0.size();

//    std::vector<std::vector<double>> J(num_residuals, std::vector<double>(num_params, 0.0));
//    double temp_params[4];

//    for (int j = 0; j < num_params; ++j) {
//        for (int i = 0; i < num_params; ++i) temp_params[i] = params[i];
//        temp_params[j] += epsilon;
//        std::vector<double> r = computeResiduals(temp_params, V, a_val);
//        for (int i = 0; i < num_residuals; ++i) {
//            J[i][j] = (r[i] - r0[i]) / epsilon;
//        }
//    }

//    return J;
//}

//// 高斯消元法（不变）
//bool BarChartMainWindow::gaussianElimination(std::vector<std::vector<double>> A, std::vector<double> b, std::vector<double>& x)
//{
//    int n = A.size();
//    if (n != b.size()) return false;

//    for (int i = 0; i < n; ++i) A[i].push_back(b[i]);

//    for (int i = 0; i < n; ++i) {
//        int max_row = i;
//        for (int j = i; j < n; ++j) {
//            if (fabs(A[j][i]) > fabs(A[max_row][i])) max_row = j;
//        }
//        if (fabs(A[max_row][i]) < 1e-12) return false;
//        std::swap(A[i], A[max_row]);

//        double div = A[i][i];
//        for (int j = i; j <= n; ++j) A[i][j] /= div;
//        for (int j = 0; j < n; ++j) {
//            if (j != i) {
//                double factor = A[j][i];
//                for (int k = i; k <= n; ++k) A[j][k] -= factor * A[i][k];
//            }
//        }
//    }

//    x.resize(n);
//    for (int i = 0; i < n; ++i) x[i] = A[i][n];
//    return true;
//}

//// 非线性最小二乘求解（放宽参数边界）
//bool BarChartMainWindow::solveNonLinearLeastSquares(const std::vector<double>& V, double a_val,
//                                                  double& k0, double& k2, double& h, double& x0)
//{
//    double params[4] = {k0, k2, h, x0};

//    // 大幅放宽参数边界（仅保留必要物理约束）
//    const double k0_low = 1e-3;    // k0允许接近0（最小0.001）
//    const double k0_high = 1e5;    // 上限足够大
//    const double k2_low = 1.0001;  // 仅要求k2>1（接近1）
//    const double k2_high = 1e3;    // 上限足够大
//    const double h_low = 1e-2;     // h允许小至0.01米
//    const double h_high = 1e2;     // 上限足够大（100米）
//    const double x0_low = -1e2;    // x0允许大范围偏移（-100米）
//    const double x0_high = 1e2;    // 上限100米

//    // LM算法参数（不变）
//    const int max_iter = 2000;
//    const double tol = 1e-12;
//    double lambda = 1e-4;
//    const double lambda_factor = 15.0;

//    double f = computeObjective(params, V, a_val);

//    for (int iter = 0; iter < max_iter; ++iter) {
//        std::vector<double> residuals = computeResiduals(params, V, a_val);
//        std::vector<std::vector<double>> J = computeJacobian(params, V, a_val);
//        int num_residuals = residuals.size();

//        std::vector<std::vector<double>> H(4, std::vector<double>(4, 0.0));
//        std::vector<double> g(4, 0.0);

//        for (int i = 0; i < 4; ++i) {
//            for (int j = 0; j < 4; ++j) {
//                for (int k = 0; k < num_residuals; ++k) {
//                    H[i][j] += J[k][i] * J[k][j];
//                }
//            }
//            for (int k = 0; k < num_residuals; ++k) {
//                g[i] += J[k][i] * residuals[k];
//            }
//        }

//        for (int i = 0; i < 4; ++i) H[i][i] += lambda;

//        std::vector<double> delta(4, 0.0);
//        bool solve_success = gaussianElimination(H, g, delta);
//        if (!solve_success) {
//            lambda *= lambda_factor;
//            continue;
//        }

//        double new_params[4];
//        for (int i = 0; i < 4; ++i) new_params[i] = params[i] - delta[i];

//        // 边界检查（仅限制极端值）
//        new_params[0] = std::max(k0_low, std::min(k0_high, new_params[0]));
//        new_params[1] = std::max(k2_low, std::min(k2_high, new_params[1]));
//        new_params[2] = std::max(h_low, std::min(h_high, new_params[2]));
//        new_params[3] = std::max(x0_low, std::min(x0_high, new_params[3]));

//        double f_new = computeObjective(new_params, V, a_val);
//        if (f_new < f) {
//            for (int i = 0; i < 4; ++i) params[i] = new_params[i];
//            f = f_new;
//            lambda /= lambda_factor;
//            double delta_norm = 0.0;
//            for (double d : delta) delta_norm += d * d;
//            if (sqrt(delta_norm) < tol || f < tol) break;
//        } else {
//            lambda *= lambda_factor;
//        }
//    }

//    k0 = params[0];
//    k2 = params[1];
//    h = params[2];
//    x0 = params[3];
//    return true;
//}

//// 批量计算（优化全零数据的初始值）
//void BarChartMainWindow::calculateForAllAValues()
//{
//    std::vector<double> V = {m_dataBuffer[0], m_dataBuffer[1], m_dataBuffer[2],
//                             m_dataBuffer[3], m_dataBuffer[4], m_dataBuffer[5],
//                             m_dataBuffer[6]};

//    qDebug() << "\n===== 输入数据（V1~V7） =====";
//    for (int i = 0; i < V.size(); ++i) {
//        qDebug() << QString("V%1: %2").arg(i + 1).arg(V[i], 0, 'f', 4);
//    }

//    std::vector<double> target_a_values = {0.2, 0.3, 0.4, 0.5};
//    qDebug() << "\n===== 开始批量计算所有a值对应的未知量 =====";

//    for (double a_val : target_a_values) {
//        qDebug() << "\n----- 计算a =" << a_val << "-----";

//        // 初始值优化：全零时k0设为极小值
//        bool isAllZero = true;
//        for (double v : V) {
//            if (v != 0) {
//                isAllZero = false;
//                break;
//            }
//        }

//        double v_max = isAllZero ? 1e-3 : *std::max_element(V.begin(), V.end());
//        double k0 = v_max * 10;       // 全零时k0≈0.001*10=0.01
//        double k2 = isAllZero ? 1.01 : 2.0; // 全零时k2接近1
//        double h = 1.5;
//        double x0 = -1.0;

//        bool success = solveNonLinearLeastSquares(V, a_val, k0, k2, h, x0);

//        qDebug() << "优化状态：" << (success ? "成功" : "失败");
//        qDebug() << "k0 =" << k0;
//        qDebug() << "k2 =" << k2;
//        qDebug() << "h =" << h << "米";
//        qDebug() << "x0 =" << x0 << "米（绝对值：" << abs(x0) << "）";
//        double params[4] = {k0, k2, h, x0}; // 栈上数组，避免内存泄漏
//        qDebug() << "误差平方和：" << 2 * computeObjective(params, V, a_val);
//    }

//    qDebug() << "\n===== 所有a值计算完成 =====";
//}

//// 单a值计算（同步优化）
//void BarChartMainWindow::calculateForCurrentAValue()
//{
//    std::vector<double> V = {m_dataBuffer[0], m_dataBuffer[1], m_dataBuffer[2],
//                             m_dataBuffer[3], m_dataBuffer[4], m_dataBuffer[5],
//                             m_dataBuffer[6]};
//    double a_val = m_a;

//    qDebug() << "\n===== 开始计算当前a值对应的未知量 =====";
//    qDebug() << "----- 计算a =" << a_val << "-----";

//    bool isAllZero = true;
//    for (double v : V) {
//        if (v != 0) {
//            isAllZero = false;
//            break;
//        }
//    }

//    double v_max = isAllZero ? 1e-3 : *std::max_element(V.begin(), V.end());
//    double k0 = v_max * 10;
//    double k2 = isAllZero ? 1.01 : 2.0;
//    double h = 1.5;
//    double x0 = -1.0;

//    bool success = solveNonLinearLeastSquares(V, a_val, k0, k2, h, x0);

//    qDebug() << "优化状态：" << (success ? "成功" : "失败");
//    qDebug() << "k0 =" << k0;
//    qDebug() << "k2 =" << k2;
//    qDebug() << "h =" << h << "米";
//    qDebug() << "x0 =" << x0 << "米（绝对值：" << abs(x0) << "）";

//    double params[4] = {k0, k2, h, x0}; // 栈上数组，避免内存泄漏
//    qDebug() << "误差平方和：" << 2 * computeObjective(params, V, a_val);

//    if (ui->label_2) {
//        ui->label_2->setText(QString("%1 米").arg(h, 0, 'f', 3));
//        ui->label_2->setStyleSheet("font-size: 12pt; font-weight: bold; color: #000000;");
//    }
//    if (ui->label_3) {
//        ui->label_3->setText(QString("%1 米").arg(x0, 0, 'f', 3));
//        ui->label_3->setStyleSheet("font-size: 12pt; font-weight: bold; color: #000000;");
//    }

//    qDebug() << "===== 当前a值计算完成 =====";
//}

//// 触发计算（保持不变）
//void BarChartMainWindow::calculateWhenEnoughData()
//{
//    //calculateForAllAValues();//all
//    calculateForCurrentAValue();//dan
//}

//// 构造函数
//BarChartMainWindow::BarChartMainWindow(QWidget *parent) :
//    QMainWindow(parent),
//    ui(new Ui::BarChartMainWindow),
//    m_a(0.2),
//    m_k0(1000.0),
//    m_k2(50.0),
//    m_h(1.0),
//    m_x0(0.0),
//    m_barCount(0),
//    m_storedGlobalMax(0.0),
//    m_simDataIndex(0) // 初始化模拟数据索引
//{
//    ui->setupUi(this);
//    // 初始化图表
//    QCustomPlot *customPlot = ui->ZZTwidget;
//    if (customPlot) {
//        customPlot->xAxis->setLabel("偏移量（米）");
//        customPlot->yAxis->setLabel("V值");
//        customPlot->xAxis->setRange(0, 6 * 0.5);  // 最大a=0.5时偏移量3米
//        customPlot->yAxis->setRange(0, 25); // 适配模拟数据最大值19，预留20%余量
//        customPlot->replot();
//    }
//    ui->label->setText(QString("%1").arg(m_a, 0, 'f', 1));

//    // 显示模拟数据提示
//    qDebug() << "===== 模拟数据模式 =====";
//    qDebug() << "待使用数据：" << SIMULATED_DATA;
//    qDebug() << "每次按下'添加数据'按钮，依次填入一个数据，满7个自动计算";
//}

//BarChartMainWindow::~BarChartMainWindow()
//{
//    delete ui;
//}

//// 减小a值（界面显示）
//void BarChartMainWindow::on_pushButton_4_clicked()
//{
//    m_a -= 0.1;
//    if (m_a < 0.2) m_a = 0.2;
//    ui->label->setText(QString("%1").arg(m_a, 0, 'f', 1));
//    qDebug() << "界面a值调整为:" << m_a;
//}

//// 增大a值（界面显示）
//void BarChartMainWindow::on_pushButton_5_clicked()
//{
//    m_a += 0.1;
//    if (m_a > 0.5) m_a = 0.5;
//    ui->label->setText(QString("%1").arg(m_a, 0, 'f', 1));
//    qDebug() << "界面a值调整为:" << m_a;
//}

//void BarChartMainWindow::setGlobalMaxFilteredData(double globalMax)
//{
//    m_storedGlobalMax = globalMax;
//   // qDebug() << "BarChartMainWindow最终接收：" << globalMax;  // 确保此行存在
//    if (ui->valueLabel) {
//        ui->valueLabel->setText(QString("强: %1").arg(globalMax, 0, 'f', 2));
//        ui->valueLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #e74c3c;");
//    }
//}

//// 返回主窗口
//void BarChartMainWindow::on_pushButton_clicked()
//{
//    emit clearGlobalMaxFilteredIntensity();
//    if (ui->valueLabel) {
//        ui->valueLabel->setText("强: 0.00");
//    }
//    QWidget *mainWindow = this->parentWidget();
//    if (mainWindow) mainWindow->show();
//    this->hide();
//}

//// 添加数据（修改为使用模拟数据，满7个时触发计算）
//void BarChartMainWindow::on_pushButton_2_clicked()
//{
//    // 检查是否还有剩余模拟数据
//    if (m_simDataIndex >= SIMULATED_DATA.size()) {
//        qDebug() << "警告：所有模拟数据已用完！（共" << SIMULATED_DATA.size() << "个）";
//        qDebug() << "请点击'清空数据'后重新开始";
//        return;
//    }

//    // 获取当前要使用的模拟数据
//    double currentData = SIMULATED_DATA[m_simDataIndex];
//    m_storedGlobalMax = currentData;
//    qDebug() << "\n添加第" << m_simDataIndex + 1 << "个数据：" << currentData;

//    QCustomPlot *customPlot = ui->ZZTwidget;
//    if (!customPlot) return;

//    // 绘制柱状图
//    QCPBars *bars = new QCPBars(customPlot->xAxis, customPlot->yAxis);
//    bars->setParent(customPlot);
//    bars->setData(QVector<double>() << m_barCount, QVector<double>() << currentData);
//    bars->setBrush(QColor(52, 152, 219));
//    bars->setPen(QPen(QColor(52, 152, 219).darker(130)));
//    bars->setWidth(0.6);

//    // 调整坐标轴（适配模拟数据范围）
//    customPlot->xAxis->setRange(-0.5, m_barCount + 0.5);
//    double newMaxY = qMax(customPlot->yAxis->range().upper, currentData * 1.2);
//    customPlot->yAxis->setRange(0, newMaxY > 0 ? newMaxY : 25);
//    customPlot->replot();

//    // 保存数据并更新索引
//    m_dataBuffer.push_back(currentData);
//    m_barCount++;
//    m_simDataIndex++;

//    // 更新UI显示当前数据
//    if (ui->valueLabel) {
//        ui->valueLabel->setText(QString("强: %1").arg(currentData, 0, 'f', 2));
//    }

//    // 数据满7个时自动触发计算
//    if (m_dataBuffer.size() >= 7) {
//        qDebug() << "\n===== 7个数据已全部添加完成，开始计算 =====";
//        calculateWhenEnoughData();
//    } else {
//        qDebug() << "剩余待添加数据：" << SIMULATED_DATA.size() - m_simDataIndex << "个";
//    }
//}

//// 清空数据（重置模拟数据索引）
//void BarChartMainWindow::on_pushButton_3_clicked()
//{
//    QCustomPlot *customPlot = ui->ZZTwidget;
//    if (customPlot) {
//        customPlot->clearPlottables();
//        customPlot->clearItems();
//        customPlot->xAxis->setRange(0, 6 * 0.5);
//        customPlot->yAxis->setRange(0, 25); // 重置为模拟数据适配范围
//        customPlot->replot();
//    }
//    m_barCount = 0;
//    m_dataBuffer.clear();
//    m_simDataIndex = 0; // 重置模拟数据索引，允许重新开始
//    m_storedGlobalMax = 0.0;

//    if (ui->valueLabel) {
//        ui->valueLabel->setText("强: 0.00");
//    }

//    qDebug() << "\n===== 数据已清空，可重新添加模拟数据 =====";
//}

#include "barchartmainwindow.h"
#include "ui_barchartmainwindow.h"
#include <QDebug>
#include <cmath>
#include <vector>


//1原始算法
// 残差计算：动态调整峰值约束权重
std::vector<double> BarChartMainWindow::computeResiduals(const double* params, const std::vector<double>& V, double a_val)
{
    double k0 = params[0];
    double k2 = params[1];
    double h = params[2];
    double x0 = params[3];
    std::vector<double> residuals;

    // 1. 数据残差（7个检测点）
    for (int i = 0; i < 7; ++i) {
        double x = a_val * i;
        double distance = sqrt(h * h + (x + x0) * (x + x0));
        double theory = (k0 * k2 * exp(-distance)) / distance;
        residuals.push_back(V[i] - theory);
    }

    // 2. 动态计算峰值约束权重（根据数据是否有特征调整）
    double dataVariance = 0.0; // 数据方差（判断是否有特征）
    double meanV = 0.0;
    for (double v : V) meanV += v;
    meanV /= V.size();
    for (double v : V) dataVariance += (v - meanV) * (v - meanV);
    dataVariance /= V.size();

    // 数据方差小时（如全零），降低峰值约束权重（最小1e-2）
    double peakWeight = std::max(1e-2, dataVariance * 1e3);

    // 峰值位置约束
    std::vector<double> theory_values;
    std::vector<double> x_offsets;
    for (int i = 0; i < 7; ++i) {
        double x = a_val * i;
        x_offsets.push_back(x);
        double distance = sqrt(h * h + (x + x0) * (x + x0));
        theory_values.push_back((k0 * k2 * exp(-distance)) / distance);
    }

    int peak_idx = 0;
    double max_val = theory_values[0];
    for (int i = 1; i < 7; ++i) {
        if (theory_values[i] > max_val) {
            max_val = theory_values[i];
            peak_idx = i;
        }
    }
    double peak_offset = x_offsets[peak_idx];
    double target_offset = 3 * a_val;
    residuals.push_back(peakWeight * (peak_offset - target_offset)); // 动态权重

    return residuals;
}

// 计算目标函数值（不变）
double BarChartMainWindow::computeObjective(const double* params, const std::vector<double>& V, double a_val)
{
    std::vector<double> residuals = computeResiduals(params, V, a_val);
    double sum = 0.0;
    for (double r : residuals) sum += r * r;
    return 0.5 * sum;
}

// 数值法计算雅克比矩阵（不变）
std::vector<std::vector<double>> BarChartMainWindow::computeJacobian(const double* params, const std::vector<double>& V, double a_val)
{
    const double epsilon = 1e-8;
    int num_params = 4;
    std::vector<double> r0 = computeResiduals(params, V, a_val);
    int num_residuals = r0.size();

    std::vector<std::vector<double>> J(num_residuals, std::vector<double>(num_params, 0.0));
    double temp_params[4];

    for (int j = 0; j < num_params; ++j) {
        for (int i = 0; i < num_params; ++i) temp_params[i] = params[i];
        temp_params[j] += epsilon;
        std::vector<double> r = computeResiduals(temp_params, V, a_val);
        for (int i = 0; i < num_residuals; ++i) {
            J[i][j] = (r[i] - r0[i]) / epsilon;
        }
    }

    return J;
}

// 高斯消元法（不变）
bool BarChartMainWindow::gaussianElimination(std::vector<std::vector<double>> A, std::vector<double> b, std::vector<double>& x)
{
    int n = A.size();
    if (n != b.size()) return false;

    for (int i = 0; i < n; ++i) A[i].push_back(b[i]);

    for (int i = 0; i < n; ++i) {
        int max_row = i;
        for (int j = i; j < n; ++j) {
            if (fabs(A[j][i]) > fabs(A[max_row][i])) max_row = j;
        }
        if (fabs(A[max_row][i]) < 1e-12) return false;
        std::swap(A[i], A[max_row]);

        double div = A[i][i];
        for (int j = i; j <= n; ++j) A[i][j] /= div;
        for (int j = 0; j < n; ++j) {
            if (j != i) {
                double factor = A[j][i];
                for (int k = i; k <= n; ++k) A[j][k] -= factor * A[i][k];
            }
        }
    }

    x.resize(n);
    for (int i = 0; i < n; ++i) x[i] = A[i][n];
    return true;
}

// 非线性最小二乘求解（放宽参数边界）
bool BarChartMainWindow::solveNonLinearLeastSquares(const std::vector<double>& V, double a_val,
                                                  double& k0, double& k2, double& h, double& x0)
{
    double params[4] = {k0, k2, h, x0};

    // 大幅放宽参数边界（仅保留必要物理约束）
    const double k0_low = 1e-3;    // k0允许接近0（最小0.001）
    const double k0_high = 1e5;    // 上限足够大
    const double k2_low = 1.0001;  // 仅要求k2>1（接近1）
    const double k2_high = 1e3;    // 上限足够大
    const double h_low = 1e-2;     // h允许小至0.01米
    const double h_high = 1e2;     // 上限足够大（100米）
    const double x0_low = -1e2;    // x0允许大范围偏移（-100米）
    const double x0_high = 1e2;    // 上限100米

    // LM算法参数（不变）
    const int max_iter = 2000;
    const double tol = 1e-12;
    double lambda = 1e-4;
    const double lambda_factor = 15.0;

    double f = computeObjective(params, V, a_val);

    for (int iter = 0; iter < max_iter; ++iter) {
        std::vector<double> residuals = computeResiduals(params, V, a_val);
        std::vector<std::vector<double>> J = computeJacobian(params, V, a_val);
        int num_residuals = residuals.size();

        std::vector<std::vector<double>> H(4, std::vector<double>(4, 0.0));
        std::vector<double> g(4, 0.0);

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                for (int k = 0; k < num_residuals; ++k) {
                    H[i][j] += J[k][i] * J[k][j];
                }
            }
            for (int k = 0; k < num_residuals; ++k) {
                g[i] += J[k][i] * residuals[k];
            }
        }

        for (int i = 0; i < 4; ++i) H[i][i] += lambda;

        std::vector<double> delta(4, 0.0);
        bool solve_success = gaussianElimination(H, g, delta);
        if (!solve_success) {
            lambda *= lambda_factor;
            continue;
        }

        double new_params[4];
        for (int i = 0; i < 4; ++i) new_params[i] = params[i] - delta[i];

        // 边界检查（仅限制极端值）
        new_params[0] = std::max(k0_low, std::min(k0_high, new_params[0]));
        new_params[1] = std::max(k2_low, std::min(k2_high, new_params[1]));
        new_params[2] = std::max(h_low, std::min(h_high, new_params[2]));
        new_params[3] = std::max(x0_low, std::min(x0_high, new_params[3]));

        double f_new = computeObjective(new_params, V, a_val);
        if (f_new < f) {
            for (int i = 0; i < 4; ++i) params[i] = new_params[i];
            f = f_new;
            lambda /= lambda_factor;
            double delta_norm = 0.0;
            for (double d : delta) delta_norm += d * d;
            if (sqrt(delta_norm) < tol || f < tol) break;
        } else {
            lambda *= lambda_factor;
        }
    }

    k0 = params[0];
    k2 = params[1];
    h = params[2];
    x0 = params[3];
    return true;
}

// 批量计算（优化全零数据的初始值）
void BarChartMainWindow::calculateForAllAValues()
{
    std::vector<double> V = {m_dataBuffer[0], m_dataBuffer[1], m_dataBuffer[2],
                             m_dataBuffer[3], m_dataBuffer[4], m_dataBuffer[5],
                             m_dataBuffer[6]};

    qDebug() << "\n===== 输入数据（V1~V7） =====";
    for (int i = 0; i < V.size(); ++i) {
        qDebug() << QString("V%1: %2").arg(i + 1).arg(V[i], 0, 'f', 4);
    }

    std::vector<double> target_a_values = {0.2, 0.3, 0.4, 0.5};
    qDebug() << "\n===== 开始批量计算所有a值对应的未知量 =====";

    for (double a_val : target_a_values) {
        qDebug() << "\n----- 计算a =" << a_val << "-----";

        // 初始值优化：全零时k0设为极小值
        bool isAllZero = true;
        for (double v : V) {
            if (v != 0) {
                isAllZero = false;
                break;
            }
        }

        double v_max = isAllZero ? 1e-3 : *std::max_element(V.begin(), V.end());
        double k0 = v_max * 10;       // 全零时k0≈0.001*10=0.01
        double k2 = isAllZero ? 1.01 : 2.0; // 全零时k2接近1
        double h = 1.5;
        double x0 = -1.0;

        bool success = solveNonLinearLeastSquares(V, a_val, k0, k2, h, x0);

        qDebug() << "优化状态：" << (success ? "成功" : "失败");
        qDebug() << "k0 =" << k0;
        qDebug() << "k2 =" << k2;
        qDebug() << "h =" << h << "米";
        qDebug() << "x0 =" << x0 << "米（绝对值：" << abs(x0) << "）";
        double* params = new double[4]{k0, k2, h, x0};
        qDebug() << "误差平方和：" << 2 * computeObjective(params, V, a_val);
        delete[] params;
    }

    qDebug() << "\n===== 所有a值计算完成 =====";
}

// 单a值计算（同步优化）
void BarChartMainWindow::calculateForCurrentAValue()
{
    std::vector<double> V = {m_dataBuffer[0], m_dataBuffer[1], m_dataBuffer[2],
                             m_dataBuffer[3], m_dataBuffer[4], m_dataBuffer[5],
                             m_dataBuffer[6]};
    double a_val = m_a;

    qDebug() << "\n===== 开始计算当前a值对应的未知量 =====";
    qDebug() << "----- 计算a =" << a_val << "-----";

    bool isAllZero = true;
    for (double v : V) {
        if (v != 0) {
            isAllZero = false;
            break;
        }
    }

    double v_max = isAllZero ? 1e-3 : *std::max_element(V.begin(), V.end());
    double k0 = v_max * 10;
    double k2 = isAllZero ? 1.01 : 2.0;
    double h = 1.5;
    double x0 = -1.0;

    bool success = solveNonLinearLeastSquares(V, a_val, k0, k2, h, x0);

    qDebug() << "优化状态：" << (success ? "成功" : "失败");
    qDebug() << "k0 =" << k0;
    qDebug() << "k2 =" << k2;
    qDebug() << "h =" << h << "米";
    qDebug() << "x0 =" << x0 << "米（绝对值：" << abs(x0) << "）";

    double* params = new double[4];
    params[0] = k0;
    params[1] = k2;
    params[2] = h;
    params[3] = x0;
    qDebug() << "误差平方和：" << 2 * computeObjective(params, V, a_val);
    delete[] params;

    if (ui->label_2) {
        ui->label_2->setText(QString("%1 米").arg(h, 0, 'f', 3));
        ui->label_2->setStyleSheet("font-size: 12pt; font-weight: bold; color: #000000;");
    }
    if (ui->label_3) {
        ui->label_3->setText(QString("%1 米").arg(x0, 0, 'f', 3));
        ui->label_3->setStyleSheet("font-size: 12pt; font-weight: bold; color: #000000;");
    }

    qDebug() << "===== 当前a值计算完成 =====";
}






// 触发计算（保持不变）
void BarChartMainWindow::calculateWhenEnoughData()
{
     //calculateForAllAValues();//all
    calculateForCurrentAValue();//dan
}



////2
//#include <algorithm> // 用于std::max_element


//#include "barchartmainwindow.h"
//#include "ui_barchartmainwindow.h"
//#include <QDebug>
//#include <cmath>
//#include <vector>
//#include <algorithm>

//// 1. 残差计算（优化公式精度+数值稳定性）
//std::vector<double> BarChartMainWindow::computeResiduals(const double* params, const std::vector<double>& V, double a_val)
//{
//    double k0 = params[0];
//    double k1 = params[1];
//    double h = params[2];
//    double x0 = params[3];
//    std::vector<double> residuals;
//    const double eps = 1e-12; // 数值稳定用微小值

//    for (int i = 0; i < 7; ++i) {
//        double x = a_val * i;
//        // 距离计算加微小值，避免后续除零或极端值
//        double distance = sqrt(h * h + (x + x0) * (x + x0)) + eps;
//        // 用exp+log替代pow，提升负指数计算精度（与Python完全对齐）
//        double k1_pow = exp(-distance * log(k1));
//        double theory = (k0 * k1_pow) / distance;
//        residuals.push_back(V[i] - theory); // 严格保持残差符号
//    }

//    return residuals;
//}

//// 计算目标函数值（不变）
//double BarChartMainWindow::computeObjective(const double* params, const std::vector<double>& V, double a_val)
//{
//    std::vector<double> residuals = computeResiduals(params, V, a_val);
//    double sum = 0.0;
//    for (double r : residuals) sum += r * r;
//    return 0.5 * sum;
//}

//// 数值法计算雅克比矩阵（不变）
//std::vector<std::vector<double>> BarChartMainWindow::computeJacobian(const double* params, const std::vector<double>& V, double a_val)
//{
//    const double epsilon = 1e-8;
//    int num_params = 4;
//    std::vector<double> r0 = computeResiduals(params, V, a_val);
//    int num_residuals = r0.size();

//    std::vector<std::vector<double>> J(num_residuals, std::vector<double>(num_params, 0.0));
//    double temp_params[4];

//    for (int j = 0; j < num_params; ++j) {
//        for (int i = 0; i < num_params; ++i) temp_params[i] = params[i];
//        temp_params[j] += epsilon;
//        std::vector<double> r = computeResiduals(temp_params, V, a_val);
//        for (int i = 0; i < num_residuals; ++i) {
//            J[i][j] = (r[i] - r0[i]) / epsilon;
//        }
//    }

//    return J;
//}

//// 高斯消元法（不变）
//bool BarChartMainWindow::gaussianElimination(std::vector<std::vector<double>> A, std::vector<double> b, std::vector<double>& x)
//{
//    int n = A.size();
//    if (n != b.size()) return false;

//    for (int i = 0; i < n; ++i) A[i].push_back(b[i]);

//    for (int i = 0; i < n; ++i) {
//        int max_row = i;
//        for (int j = i; j < n; ++j) {
//            if (fabs(A[j][i]) > fabs(A[max_row][i])) max_row = j;
//        }
//        if (fabs(A[max_row][i]) < 1e-12) return false;
//        std::swap(A[i], A[max_row]);

//        double div = A[i][i];
//        for (int j = i; j <= n; ++j) A[i][j] /= div;
//        for (int j = 0; j < n; ++j) {
//            if (j != i) {
//                double factor = A[j][i];
//                for (int k = i; k <= n; ++k) A[j][k] -= factor * A[i][k];
//            }
//        }
//    }

//    x.resize(n);
//    for (int i = 0; i < n; ++i) x[i] = A[i][n];
//    return true;
//}

//// 2. 非线性最小二乘求解（关键优化：延迟裁剪+匹配Python收敛特性）
//bool BarChartMainWindow::solveNonLinearLeastSquares(const std::vector<double>& V, double a_val,
//                                                  double& k0, double& k2, double& h, double& x0)
//{
//    // 强制使用Python初始值，完全覆盖外部传入参数
//    double params[4] = {10000.0, 3.0, 2.0, -1.0};
//    // 严格匹配Python参数边界
//    const double k0_low = 100.0,  k0_high = 1e6;
//    const double k1_low = 1.0,    k1_high = 10.0;
//    const double h_low = 0.0,     h_high = 10.0;
//    const double x0_low = -5.0,   x0_high = 5.0;

//    // LM算法参数：模拟Python信赖域方法的收敛特性
//    const int max_iter = 5000;    // 增加迭代次数，确保充分收敛
//    const double tol = 1e-9;      // 提高精度，避免过早停止
//    double lambda = 1e-5;         // 更小初始阻尼，允许更大搜索步长
//    const double lambda_factor = 20.0; // 平缓调整阻尼，避免跳过最优解

//    double f = computeObjective(params, V, a_val);

//    for (int iter = 0; iter < max_iter; ++iter) {
//        std::vector<double> residuals = computeResiduals(params, V, a_val);
//        std::vector<std::vector<double>> J = computeJacobian(params, V, a_val);
//        int num_residuals = residuals.size();

//        // 计算Hessian矩阵和梯度
//        std::vector<std::vector<double>> H(4, std::vector<double>(4, 0.0));
//        std::vector<double> g(4, 0.0);
//        for (int i = 0; i < 4; ++i) {
//            for (int j = 0; j < 4; ++j) {
//                for (int k = 0; k < num_residuals; ++k) {
//                    H[i][j] += J[k][i] * J[k][j];
//                }
//            }
//            for (int k = 0; k < num_residuals; ++k) {
//                g[i] += J[k][i] * residuals[k];
//            }
//        }

//        // 添加阻尼项
//        for (int i = 0; i < 4; ++i) H[i][i] += lambda;

//        // 求解线性方程组
//        std::vector<double> delta(4, 0.0);
//        bool solve_success = gaussianElimination(H, g, delta);
//        if (!solve_success) {
//            lambda *= lambda_factor;
//            continue;
//        }

//        // 关键修改：迭代中不裁剪参数，保留完整搜索空间（避免过早限制h）
//        double new_params[4];
//        for (int i = 0; i < 4; ++i) {
//            new_params[i] = params[i] - delta[i];
//        }

//        // 计算新目标函数值
//        double f_new = computeObjective(new_params, V, a_val);
//        if (f_new < f) {
//            // 接受新参数，更新状态
//            for (int i = 0; i < 4; ++i) params[i] = new_params[i];
//            f = f_new;
//            lambda /= lambda_factor;

//            // 收敛判断：参数变化量+目标函数值双重判定
//            double delta_norm = 0.0;
//            for (double d : delta) delta_norm += d * d;
//            if (sqrt(delta_norm) < tol || f < tol) break;
//        } else {
//            // 拒绝新参数，增大阻尼
//            lambda *= lambda_factor;
//        }
//    }

//    // 最终裁剪参数（仅在迭代结束后约束，确保搜索充分）
//    params[0] = std::max(k0_low, std::min(k0_high, params[0]));
//    params[1] = std::max(k1_low, std::min(k1_high, params[1]));
//    params[2] = std::max(h_low, std::min(h_high, params[2])); // 最后约束h
//    params[3] = std::max(x0_low, std::min(x0_high, params[3]));

//    // 赋值输出（保持外部接口兼容性）
//    k0 = params[0];
//    k2 = params[1]; // k1值赋给k2，不修改外部调用逻辑
//    h = params[2];
//    x0 = params[3];
//    return true;
//}

//// 3. 批量计算（适配新算法，保留原接口）
//void BarChartMainWindow::calculateForAllAValues()
//{
//    std::vector<double> V = {m_dataBuffer[0], m_dataBuffer[1], m_dataBuffer[2],
//                             m_dataBuffer[3], m_dataBuffer[4], m_dataBuffer[5],
//                             m_dataBuffer[6]};

//    qDebug() << "\n===== 输入数据（V1~V7） =====";
//    for (int i = 0; i < V.size(); ++i) {
//        qDebug() << QString("V%1: %2").arg(i + 1).arg(V[i], 0, 'f', 4);
//    }

//    std::vector<double> target_a_values = {0.2, 0.3, 0.4, 0.5};
//    qDebug() << "\n===== 开始批量计算所有a值对应的未知量 =====";

//    for (double a_val : target_a_values) {
//        qDebug() << "\n----- 计算a =" << a_val << "-----";

//        // 全零数据判断（保留原逻辑）
//        bool isAllZero = true;
//        for (double v : V) {
//            if (fabs(v) > 1e-12) {
//                isAllZero = false;
//                break;
//            }
//        }

//        // 初始值仅用于函数调用（算法内已强制Python初始值，此处不影响）
//        double v_max = isAllZero ? 1e-3 : *std::max_element(V.begin(), V.end());
//        double k0 = v_max * 10;
//        double k2 = isAllZero ? 1.01 : 2.0;
//        double h = 1.0;
//        double x0 = -1.0;

//        bool success = solveNonLinearLeastSquares(V, a_val, k0, k2, h, x0);

//        // 输出结果（标注k2对应Python的k1）
//        qDebug() << "优化状态：" << (success ? "成功" : "失败");
//        qDebug() << QString("k0 = %1").arg(k0, 0, 'f', 4);
//        qDebug() << QString("k1（原k2） = %1").arg(k2, 0, 'f', 4);
//        qDebug() << QString("h = %1 米").arg(h, 0, 'f', 4);
//        qDebug() << QString("x0 = %1 米（绝对值：%2）").arg(x0, 0, 'f', 4).arg(fabs(x0), 0, 'f', 4);

//        // 计算残差平方和（与Python result.cost*2完全一致）
//        double params[4] = {k0, k2, h, x0};
//        double min_delta = 2 * computeObjective(params, V, a_val);
//        qDebug() << QString("最小残差平方和 δ = %1").arg(min_delta, 0, 'f', 4);
//    }

//    qDebug() << "\n===== 所有a值计算完成 =====";
//}

//// 4. 单a值计算（适配新算法，保留UI更新）
//void BarChartMainWindow::calculateForCurrentAValue()
//{
//    std::vector<double> V = {m_dataBuffer[0], m_dataBuffer[1], m_dataBuffer[2],
//                             m_dataBuffer[3], m_dataBuffer[4], m_dataBuffer[5],
//                             m_dataBuffer[6]};
//    double a_val = m_a;

//    qDebug() << "\n===== 开始计算当前a值对应的未知量 =====";
//    qDebug() << "----- 计算a =" << a_val << "-----";

//    bool isAllZero = true;
//    for (double v : V) {
//        if (fabs(v) > 1e-12) {
//            isAllZero = false;
//            break;
//        }
//    }

//    // 初始值仅用于函数调用（算法内已强制Python初始值）
//    double v_max = isAllZero ? 1e-3 : *std::max_element(V.begin(), V.end());
//    double k0 = v_max * 10;
//    double k2 = isAllZero ? 1.01 : 2.0;
//    double h = 1.5;
//    double x0 = -1.0;

//    bool success = solveNonLinearLeastSquares(V, a_val, k0, k2, h, x0);

//    // 输出结果
//    qDebug() << "优化状态：" << (success ? "成功" : "失败");
//    qDebug() << QString("k0 = %1").arg(k0, 0, 'f', 4);
//    qDebug() << QString("k1（原k2） = %1").arg(k2, 0, 'f', 4);
//    qDebug() << QString("h = %1 米").arg(h, 0, 'f', 4);
//    qDebug() << QString("x0 = %1 米（绝对值：%2）").arg(x0, 0, 'f', 4).arg(fabs(x0), 0, 'f', 4);

//    // 计算残差平方和
//    double params[4] = {k0, k2, h, x0};
//    double min_delta = 2 * computeObjective(params, V, a_val);
//    qDebug() << QString("最小残差平方和 δ = %1").arg(min_delta, 0, 'f', 4);
//    // 修复内存泄漏：原代码new后未delete，此处直接用栈上数组
//    // delete[] params;  // 注释掉，因params改为栈上数组

//    // 更新UI显示
//    if (ui->label_2) {
//        ui->label_2->setText(QString("%1 米").arg(h, 0, 'f', 3));
//        ui->label_2->setStyleSheet("font-size: 12pt; font-weight: bold; color: #000000;");
//    }
//    if (ui->label_3) {
//        ui->label_3->setText(QString("%1 米").arg(x0, 0, 'f', 3));
//        ui->label_3->setStyleSheet("font-size: 12pt; font-weight: bold; color: #000000;");
//    }

//    qDebug() << "===== 当前a值计算完成 =====";
//}

//// 触发计算（保持不变）
//void BarChartMainWindow::calculateWhenEnoughData()
//{
//    calculateForAllAValues();//all
//    //calculateForCurrentAValue();//dan
//}


// 构造函数
BarChartMainWindow::BarChartMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BarChartMainWindow),
    m_a(0.2),
    m_k0(1000.0),
    m_k2(50.0),
    m_h(1.0),
    m_x0(0.0),
    m_barCount(0),
    m_storedGlobalMax(0.0)
{
    ui->setupUi(this);
    // 初始化图表
    QCustomPlot *customPlot = ui->ZZTwidget;
    if (customPlot) {
        customPlot->xAxis->setLabel("偏移量（米）");
        customPlot->yAxis->setLabel("V值");
        customPlot->xAxis->setRange(0, 6 * 0.5);  // 最大a=0.5时偏移量3米
        customPlot->yAxis->setRange(0, 100);
        customPlot->replot();
    }
    ui->label->setText(QString("%1").arg(m_a, 0, 'f', 1));
}

BarChartMainWindow::~BarChartMainWindow()
{
    delete ui;
}

// 减小a值（界面显示）
void BarChartMainWindow::on_pushButton_4_clicked()
{
    m_a -= 0.1;
    if (m_a < 0.2) m_a = 0.2;
    ui->label->setText(QString("%1").arg(m_a, 0, 'f', 1));
    qDebug() << "界面a值调整为:" << m_a;
}

// 增大a值（界面显示）
void BarChartMainWindow::on_pushButton_5_clicked()
{
    m_a += 0.1;
    if (m_a > 0.5) m_a = 0.5;
    ui->label->setText(QString("%1").arg(m_a, 0, 'f', 1));
    qDebug() << "界面a值调整为:" << m_a;
}

void BarChartMainWindow::setGlobalMaxFilteredData(double globalMax)
{
    m_storedGlobalMax = globalMax;
   // qDebug() << "BarChartMainWindow最终接收：" << globalMax;  // 确保此行存在
    if (ui->valueLabel) {
        ui->valueLabel->setText(QString("强: %1").arg(globalMax, 0, 'f', 2));
        ui->valueLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #e74c3c;");
    }
}

// 返回主窗口
void BarChartMainWindow::on_pushButton_clicked()
{
    emit clearGlobalMaxFilteredIntensity();
    if (ui->valueLabel) {
        ui->valueLabel->setText("强: 0.00");
    }
    QWidget *mainWindow = this->parentWidget();
    if (mainWindow) mainWindow->show();
    this->hide();
}

// 添加数据（满7个时触发计算）
void BarChartMainWindow::on_pushButton_2_clicked()
{
    if (m_storedGlobalMax < 0) return;

    QCustomPlot *customPlot = ui->ZZTwidget;
    if (!customPlot) return;

    // 绘制柱状图
    QCPBars *bars = new QCPBars(customPlot->xAxis, customPlot->yAxis);
    bars->setParent(customPlot);
    bars->setData(QVector<double>() << m_barCount, QVector<double>() << m_storedGlobalMax);
    bars->setBrush(QColor(52, 152, 219));
    bars->setPen(QPen(QColor(52, 152, 219).darker(130)));
    bars->setWidth(0.6);

    // 调整坐标轴
    customPlot->xAxis->setRange(-0.5, m_barCount + 0.5);
    double newMaxY = qMax(customPlot->yAxis->range().upper, m_storedGlobalMax * 1.2);
    customPlot->yAxis->setRange(0, newMaxY > 0 ? newMaxY : 100);
    customPlot->replot();

    // 保存数据
    m_dataBuffer.push_back(m_storedGlobalMax);
    m_barCount++;

    // 数据满7个时自动触发计算
    if (m_dataBuffer.size() >= 7) {
        calculateWhenEnoughData();
    }
}

// 清空数据
void BarChartMainWindow::on_pushButton_3_clicked()
{
    QCustomPlot *customPlot = ui->ZZTwidget;
    if (customPlot) {
        customPlot->clearPlottables();
        customPlot->clearItems();
        customPlot->xAxis->setRange(0, 6 * 0.5);
        customPlot->yAxis->setRange(0, 100);
        customPlot->replot();
    }
    m_barCount = 0;
    m_dataBuffer.clear();
}
