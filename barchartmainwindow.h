#ifndef BARCHARTMAINWINDOW_H
#define BARCHARTMAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h" // 包含QCustomPlot头文件
#include <vector>  // 用于存储7个数值

namespace Ui {
class BarChartMainWindow;
}

class BarChartMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BarChartMainWindow(QWidget *parent = nullptr);
    ~BarChartMainWindow();

    void setGlobalMaxFilteredData(double globalMax);

    // 新增：优化相关成员
    double m_k0 = 1.0;    // 初始猜测值，可根据实际调整
    double m_k2 = 1.0;
    double m_h  = 1.0;
    double m_x0 = 0.0;

    // 添加自定义clamp函数，放在文件开头或类的公有部分
    template <typename T>
    const T& custom_clamp(const T& value, const T& min_val, const T& max_val) {
        return std::max(min_val, std::min(value, max_val));
    }



private slots:
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_5_clicked();

signals:
    // 声明清除全局最大滤强的信号
    void clearGlobalMaxFilteredIntensity();

private:
    Ui::BarChartMainWindow *ui;
    // 添加模拟数据索引变量（关键！）
      int m_simDataIndex;
    QCPBars *m_bars; // 柱状图对象
    double m_storedGlobalMax; // 存储全局最大值
    int m_barCount; // 柱状图计数变量
    QVector<double> m_dataBuffer; // 数据缓冲区（存储7个输入值）
    double m_a = 0.4; // 当前a值

    // 新增：优化算法相关函数声明

    // 新增：声明optimizeWithInitialParams函数
        double optimizeWithInitialParams(const std::vector<double>& V, double a_val,
                                        double initial_k0, double initial_k2, double initial_h, double initial_x0,
                                        double& out_k0, double& out_k2, double& out_h, double& out_kx0);

    /**
     * @brief 计算残差（实测值与理论值的偏差+约束项）
     * @param params 待优化参数[k0, k2, h, x0]
     * @param V 实测V值数组（7个元素）
     * @param a_val 当前a值
     * @return 残差向量
     */
    std::vector<double> computeResiduals(const double* params, const std::vector<double>& V, double a_val);

    /**
     * @brief 计算目标函数值（残差平方和的一半）
     * @param params 待优化参数
     * @param V 实测V值数组
     * @param a_val 当前a值
     * @return 目标函数值
     */
    double computeObjective(const double* params, const std::vector<double>& V, double a_val);

    /**
     * @brief 数值法计算雅克比矩阵
     * @param params 待优化参数
     * @param V 实测V值数组
     * @param a_val 当前a值
     * @return 雅克比矩阵（残差数×参数数）
     */
    std::vector<std::vector<double>> computeJacobian(const double* params, const std::vector<double>& V, double a_val);

    /**
     * @brief 高斯消元法求解线性方程组Ax = b
     * @param A 系数矩阵
     * @param b 常数项向量
     * @param x 解向量（输出）
     * @return 是否求解成功
     */
    bool gaussianElimination(std::vector<std::vector<double>> A, std::vector<double> b, std::vector<double>& x);

    /**
     * @brief 非线性最小二乘求解（Levenberg-Marquardt算法）
     * @param V 实测V值数组
     * @param a_val 当前a值
     * @param k0 输出参数k0
     * @param k2 输出参数k2
     * @param h 输出参数h（深度）
     * @param x0 输出参数x0
     * @return 是否优化成功
     */
    bool solveNonLinearLeastSquares(const std::vector<double>& V, double a_val,
                                  double& k0, double& k2, double& h, double& x0);

    /**
     * @brief 批量计算多个a值对应的未知量
     * @note 计算a=0.2,0.3,0.4,0.5时的k0,k2,h,x0
     */
    void calculateForAllAValues();

    /**
     * @brief 计算当前界面显示的a值对应的未知量
     * @note 使用m_a作为当前a值
     */
    void calculateForCurrentAValue();

    /**
     * @brief 数据满7个时触发计算（切换批量/单a值计算）
     */
    void calculateWhenEnoughData();
};

#endif // BARCHARTMAINWINDOW_H
