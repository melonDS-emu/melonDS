#include "ShaderConfigDialog.h"
#include "Window.h"
#include "Config.h"
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QSlider>
#include <QFormLayout>
#include <QFrame>
#include <QEvent>
#include <QPushButton>

ShaderConfigDialog::ShaderConfigDialog(ShaderManager* manager, const std::vector<FrontendShader>& presets, MainWindow* mainWindow)
    : QDialog(mainWindow), m_shaderManager(manager), m_presets(presets), m_mainWindow(mainWindow) {
    setWindowTitle("Shader Configuration");
    setMinimumSize(700, 500);

    if (m_mainWindow) {
        std::string savedParamsStr = m_mainWindow->getWindowConfig().GetString("ShaderParams");
        std::stringstream ssOuter(savedParamsStr);
        std::string presetBlock;
        while (std::getline(ssOuter, presetBlock, '#')) {
            size_t pipePos = presetBlock.find('|');
            if (pipePos != std::string::npos) {
                std::string presetPath = presetBlock.substr(0, pipePos);
                std::string paramsStr = presetBlock.substr(pipePos + 1);
                std::stringstream ssInner(paramsStr);
                std::string paramPair;
                while (std::getline(ssInner, paramPair, ';')) {
                    size_t eqPos = paramPair.find('=');
                    if (eqPos != std::string::npos) {
                        try {
                            float val = std::stof(paramPair.substr(eqPos + 1));
                            m_savedParameters[presetPath][paramPair.substr(0, eqPos)] = val;
                        } catch(...) {}
                    }
                }
            }
        }
    }

    // Restore previously saved parameter values from config
    setupUI();
    loadPresets();
}

bool ShaderConfigDialog::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Wheel && qobject_cast<QSlider*>(obj)) {
        return true; // Ignore wheel events on sliders
    }
    return QDialog::eventFilter(obj, event);
}

void ShaderConfigDialog::setupUI() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    m_presetList = new QListWidget(this);
    connect(m_presetList, &QListWidget::itemClicked, this, &ShaderConfigDialog::onPresetSelected);
    connect(m_presetList, &QListWidget::itemChanged, this, &ShaderConfigDialog::onPresetChanged);
    connect(m_presetList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem* previous) {
        if (current) onPresetSelected(current);
    });
    splitter->addWidget(m_presetList);

    QWidget* rightWidget = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);

    m_titleLabel = new QLabel("<b>Shader Title</b>", this);
    m_titleLabel->setStyleSheet("font-size: 16px;");
    m_authorLabel = new QLabel("Author: Unknown", this);
    m_descLabel = new QLabel("No description available.", this);
    m_descLabel->setWordWrap(true);

    m_resetButton = new QPushButton("Reset to Default", this);
    m_resetButton->setVisible(false);
    connect(m_resetButton, &QPushButton::clicked, this, &ShaderConfigDialog::onResetParameters);

    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_resetButton);

    rightLayout->addLayout(titleLayout);
    rightLayout->addWidget(m_authorLabel);
    rightLayout->addWidget(m_descLabel);

    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    rightLayout->addWidget(line);

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    m_paramsWidget = new QWidget(this);
    m_paramsLayout = new QVBoxLayout(m_paramsWidget);
    m_paramsLayout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(m_paramsWidget);
    rightLayout->addWidget(scrollArea);

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
}

void ShaderConfigDialog::loadPresets() {
    m_presetList->blockSignals(true);

    std::string currentPathsStr = "";
    if (m_mainWindow) {
        currentPathsStr = m_mainWindow->getWindowConfig().GetString("ShaderPresetPath");
    }

    std::vector<std::string> currentPaths;
    {
        std::stringstream ss(currentPathsStr);
        std::string part;
        while (std::getline(ss, part, '+')) {
            if (!part.empty()) currentPaths.push_back(part);
        }
    }

    QListWidgetItem* nearestItem = new QListWidgetItem("Nearest (Default)", m_presetList);
    nearestItem->setFlags(nearestItem->flags() | Qt::ItemIsUserCheckable);
    nearestItem->setCheckState(currentPaths.empty() ? Qt::Checked : Qt::Unchecked);

    for (const auto& feShader : m_presets) {
        QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(feShader.name), m_presetList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        bool isChecked = std::find(currentPaths.begin(), currentPaths.end(), feShader.path) != currentPaths.end();
        item->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked);

        if (isChecked) {
            updateDetails(feShader);
        }
    }

    if (currentPaths.empty()) {
        m_titleLabel->setText("<b>Nearest (Default)</b>");
        m_authorLabel->setText("Author: N/A");
        m_descLabel->setText("Standard nearest-neighbor scaling without filters.");
    }

    m_presetList->blockSignals(false);
}

void ShaderConfigDialog::onPresetSelected(QListWidgetItem* item) {
    int index = m_presetList->row(item);

    if (index == 0) {
        m_titleLabel->setText("<b>Nearest (Default)</b>");
        m_authorLabel->setText("Author: N/A");
        m_descLabel->setText("Standard nearest-neighbor scaling without filters.");
        clearLayout(m_paramsLayout);
        m_resetButton->setVisible(false);
    } else {
        const auto& feShader = m_presets[index - 1];
        updateDetails(feShader);
    }
}

void ShaderConfigDialog::onPresetChanged(QListWidgetItem* item) {
    SlangPreset combined;
    bool anyShaderChecked = false;
    std::string pathsStr = "";

    for (int i = 1; i < m_presetList->count(); ++i) {
        QListWidgetItem* curItem = m_presetList->item(i);
        if (curItem->checkState() == Qt::Checked) {
            anyShaderChecked = true;
            const auto& feShader = m_presets[i - 1];
            if (combined.getPasses().empty()) {
                combined = feShader.preset;
            } else {
                combined.appendPassesFrom(feShader.preset);
            }
            if (!pathsStr.empty()) pathsStr += "+";
            pathsStr += feShader.path;
        }
    }

    m_presetList->blockSignals(true);
    m_presetList->item(0)->setCheckState(anyShaderChecked ? Qt::Unchecked : Qt::Checked);

    m_presetList->setCurrentItem(item); // Keep checked item highlighted in blue
    onPresetSelected(item);
    m_presetList->blockSignals(false);

    m_shaderManager->RequestPresetLoad(anyShaderChecked ? combined : SlangPreset());

    if (m_mainWindow) {
        m_mainWindow->getWindowConfig().SetString("ShaderPresetPath", pathsStr);
        Config::Save();
    }
}

void ShaderConfigDialog::updateDetails(const FrontendShader& feShader) {
    m_titleLabel->setText("<b>" + QString::fromStdString(feShader.name) + "</b>");

    if (!feShader.author.empty()) m_authorLabel->setText("Author: " + QString::fromStdString(feShader.author));
    else m_authorLabel->setText("Author: Unknown");

    if (!feShader.description.empty()) m_descLabel->setText(QString::fromStdString(feShader.description));
    else m_descLabel->setText("No description available.");

    clearLayout(m_paramsLayout);

    bool hasParams = false;

    for (const auto& param : feShader.preset.getPragmaParameters()) {
        hasParams = true;
        QGroupBox* group = new QGroupBox(QString::fromStdString(param.description), m_paramsWidget);
        QFormLayout* form = new QFormLayout(group);

        float valToUse = param.defaultValue;
        if (m_savedParameters.count(feShader.path) && m_savedParameters[feShader.path].count(param.name)) {
            valToUse = m_savedParameters[feShader.path][param.name];
        }

        QLabel* valLabel = new QLabel(QString::number(valToUse, 'f', 2), group);
        QSlider* slider = new QSlider(Qt::Horizontal, group);
        slider->installEventFilter(this);

        slider->setRange(0, 1000);
        int startVal = (int)((valToUse - param.minValue) / (param.maxValue - param.minValue) * 1000.0f);

        slider->setValue(qBound(0, startVal, 1000));

        connect(slider, &QSlider::valueChanged, [this, param, valLabel, presetPath = feShader.path](int val) {
            float floatVal = param.minValue + (float)val / 1000.0f * (param.maxValue - param.minValue);
            valLabel->setText(QString::number(floatVal, 'f', 2));
            onParameterChanged(param.name, floatVal);

            m_savedParameters[presetPath][param.name] = floatVal;
        });

        form->addRow("Value:", valLabel);
        form->addRow(slider);
        m_paramsLayout->addWidget(group);

        if (valToUse != param.defaultValue) {
            onParameterChanged(param.name, valToUse);
        }
    }

    for (const auto& [name, defaultValue] : feShader.preset.getParameters()) {
        hasParams = true;
        QGroupBox* group = new QGroupBox(QString::fromStdString(name), m_paramsWidget);
        QFormLayout* form = new QFormLayout(group);

        float valToUse = defaultValue;
        if (m_savedParameters.count(feShader.path) && m_savedParameters[feShader.path].count(name)) {
            valToUse = m_savedParameters[feShader.path][name];
        }

        QLabel* valLabel = new QLabel(QString::number(valToUse, 'f', 2), group);
        form->addRow("Value:", valLabel);
        m_paramsLayout->addWidget(group);

        if (valToUse != defaultValue) {
            onParameterChanged(name, valToUse);
        }
    }

    m_resetButton->setVisible(hasParams);

    connect(this, &QDialog::finished, [this, mainWin = m_mainWindow](int result){
        if (mainWin) {
            std::string str = "";
            for (const auto& [pPath, pMap] : m_savedParameters) {
                if (pMap.empty()) continue;
                str += pPath + "|";
                for (const auto& [n, v] : pMap) {
                    str += n + "=" + std::to_string(v) + ";";
                }
                str += "#";
            }
            mainWin->getWindowConfig().SetString("ShaderParams", str);
            Config::Save();
        }
    });
}

void ShaderConfigDialog::onParameterChanged(const std::string& name, float value) {
    m_shaderManager->SetParameter(name, value);
}

void ShaderConfigDialog::onResetParameters() {
    std::string currentPath = "";
    int currentIndex = m_presetList->currentRow();
    if (currentIndex > 0) {
        currentPath = m_presets[currentIndex - 1].path;
    }

    if (currentPath.empty()) return;

    m_savedParameters.erase(currentPath); // Clear saved values and reload defaults

    const auto& feShader = m_presets[currentIndex - 1];

    for (const auto& param : feShader.preset.getPragmaParameters()) {
        onParameterChanged(param.name, param.defaultValue);
    }
    for (const auto& [name, defaultValue] : feShader.preset.getParameters()) {
        onParameterChanged(name, defaultValue);
    }

    updateDetails(feShader);
}

void ShaderConfigDialog::clearLayout(QLayout* layout) {
    if (!layout) return;
    // Recursively delete all widgets and sub-layouts
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        if (QLayout* childLayout = item->layout()) {
            clearLayout(childLayout);
        }
        delete item;
    }
}