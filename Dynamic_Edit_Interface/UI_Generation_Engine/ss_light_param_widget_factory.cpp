#include "ss_light_param_widget_factory.h"
#include "moc/moc_ss_light_param_widget_factory.cpp"

#include <QtWidgets/QWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtCore/QObject>

#include "../../include/Parsing_Engine/ss_light_resource_models.h"

static std::string ToStdString(const QString& s) { return s.toStdString(); }
static QString ToQString(const std::string& s) { return QString::fromStdString(s); }

static int SafeToInt(const std::string& s, int def)
{
    try { return std::stoi(s); }
    catch (...) { return def; }
}
static double SafeToDouble(const std::string& s, double def)
{
    try { return std::stod(s); }
    catch (...) { return def; }
}

SS_LightWidgetHandle SS_LightParamWidgetFactory::Create(const SS_LightParamDef& def, QWidget* parent)
{
    SS_LightWidgetHandle h;

    h.emitter = new SS_LightWidgetChangeEmitter(parent);

    // widget behavior flags
    const bool enabled = def.widget.enabled;
    const bool visible = def.widget.visible;
    const bool read_only = def.widget.read_only;

    auto apply_common = [&](QWidget* w)
    {
        if (!w) return;
        w->setEnabled(enabled);
        w->setVisible(visible);
        if (!def.widget.tooltip.empty())
            w->setToolTip(ToQString(def.widget.tooltip));
    };

    switch (def.widget.type)
    {
    case SS_LIGHT_WIDGET_TYPE::SPIN_BOX:
    {
        auto* w = new QSpinBox(parent);
        apply_common(w);

        w->setMinimum((int)def.widget.min_value);
        w->setMaximum((int)def.widget.max_value);
        w->setSingleStep((int)def.widget.step);

        // QAbstractSpinBox 提供 setReadOnly（public）
        w->setReadOnly(read_only);

        // 初始值
        const int v0 = SafeToInt(def.widget.default_value, w->minimum());
        w->setValue(v0);

        // 取/设
        h.get_value = [w]() { return std::to_string(w->value()); };
        h.set_value = [w](const std::string& s) {QSignalBlocker b(w); w->setValue(SafeToInt(s, w->value())); };

        // 信号：用 editingFinished，避免拖动时疯狂发送
        QObject::connect(w, &QSpinBox::editingFinished, h.emitter, &SS_LightWidgetChangeEmitter::SignalChanged);

        h.widget = w;
        break;
    }
    case SS_LIGHT_WIDGET_TYPE::DOUBLE_SPIN_BOX:
    {
        auto* w = new QDoubleSpinBox(parent);
        apply_common(w);

        w->setMinimum(def.widget.min_value);
        w->setMaximum(def.widget.max_value);
        w->setSingleStep(def.widget.step);
        w->setDecimals(def.widget.decimals);

        // QAbstractSpinBox 提供 setReadOnly（public）
        w->setReadOnly(read_only);

        const double v0 = SafeToDouble(def.widget.default_value, w->minimum());
        w->setValue(v0);

        h.get_value = [w]()
        {
            // 直接用 QString 格式，避免科学计数法鬼畜
            return ToStdString(QString::number(w->value(), 'f', w->decimals()));
        };
        h.set_value = [w](const std::string& s) {QSignalBlocker b(w); w->setValue(SafeToDouble(s, w->value())); };

        QObject::connect(w, &QDoubleSpinBox::editingFinished, h.emitter, &SS_LightWidgetChangeEmitter::SignalChanged);

        h.widget = w;
        break;
    }
    case SS_LIGHT_WIDGET_TYPE::CHECK_BOX:
    {
        auto* w = new QCheckBox(parent);
        apply_common(w);

        // checkbox 的 read_only：禁用交互但保留显示，通常 setEnabled(false)；这里保留 enabled 逻辑
        // 初始值
        const std::string dv = def.widget.default_value.empty() ? def.default_value : def.widget.default_value;
        const bool checked = (dv == "1" || dv == "true" || dv == "True" || dv == "TRUE");
        w->setChecked(checked);

        h.get_value = [w]() { return w->isChecked() ? "true" : "false"; };
        h.set_value = [w](const std::string& s)
        {
            QSignalBlocker b(w);
            const bool c = (s == "1" || s == "true" || s == "True" || s == "TRUE");
            w->setChecked(c);
        };

        QObject::connect(w, &QCheckBox::toggled, h.emitter, &SS_LightWidgetChangeEmitter::SignalChanged);

        h.widget = w;
        break;
    }
    case SS_LIGHT_WIDGET_TYPE::COMBO_BOX:
    {
        auto* w = new QComboBox(parent);
        apply_common(w);
        w->setEditable(false);

        for (const auto& opt : def.widget.options)
            w->addItem(ToQString(opt));

        const std::string dv = def.widget.default_value.empty() ? def.default_value : def.widget.default_value;
        const int idx = w->findText(ToQString(dv));
        if (idx >= 0) w->setCurrentIndex(idx);
        else if (w->count() > 0) w->setCurrentIndex(0);

        h.get_value = [w]() { return ToStdString(w->currentText()); };
        h.set_value = [w](const std::string& s)
        {
            QSignalBlocker b(w);
            const int i = w->findText(ToQString(s));
            if (i >= 0) w->setCurrentIndex(i);
        };

        QObject::connect(
            w,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            h.emitter,
            &SS_LightWidgetChangeEmitter::SignalChanged
        );


        h.widget = w;
        break;
    }
    case SS_LIGHT_WIDGET_TYPE::LINE_EDIT:
    default:
    {
        auto* w = new QLineEdit(parent);
        apply_common(w);

        w->setReadOnly(read_only);
        if (def.widget.text_max_length > 0)
            w->setMaxLength(def.widget.text_max_length);
        if (!def.widget.placeholder.empty())
            w->setPlaceholderText(ToQString(def.widget.placeholder));

        const std::string dv = def.widget.default_value.empty() ? def.default_value : def.widget.default_value;
        w->setText(ToQString(dv));

        h.get_value = [w]() { return ToStdString(w->text()); };
        h.set_value = [w](const std::string& s) {QSignalBlocker b(w); w->setText(ToQString(s)); };

        QObject::connect(w, &QLineEdit::editingFinished, h.emitter, &SS_LightWidgetChangeEmitter::SignalChanged);

        h.widget = w;
        break;
    }
    }

    return h;
}
