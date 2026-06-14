#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <memory>
#include "FrontendShader.h"
#include "ShaderManager.h"

class MainWindow;

class ShaderConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit ShaderConfigDialog(ShaderManager* manager, const std::vector<FrontendShader>& presets, MainWindow* mainWindow);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onPresetSelected(QListWidgetItem* item);
    // Called when a checkbox is toggled — rebuilds the combined preset and reloads
    void onPresetChanged(QListWidgetItem* item);
    void onParameterChanged(const std::string& name, float value);
    void onResetParameters();

private:
    void setupUI();
    void loadPresets();
    void updateDetails(const FrontendShader& feShader);
    void clearLayout(QLayout* layout);

    ShaderManager* m_shaderManager;
    std::vector<FrontendShader> m_presets;
    MainWindow* m_mainWindow;

    QListWidget* m_presetList;
    QLabel* m_titleLabel;
    QLabel* m_authorLabel;
    QLabel* m_descLabel;
    QWidget* m_paramsWidget;
    QVBoxLayout* m_paramsLayout;
    QPushButton* m_resetButton;
    
    // User-modified parameter values per preset path
    std::map<std::string, std::map<std::string, float>> m_savedParameters;
};
