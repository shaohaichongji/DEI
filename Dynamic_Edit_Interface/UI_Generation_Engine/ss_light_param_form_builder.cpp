#include "ss_light_param_form_builder.h"
#include "moc/moc_ss_light_param_form_builder.cpp"

#include <QtWidgets/QScrollArea>
#include <QtWidgets/QWidget>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>

#include "../../include/Parsing_Engine/ss_light_resource_models.h"
#include "../../include/Parsing_Engine/ss_light_resource_types.h"

SS_LightParamFormBuilder::SS_LightParamFormBuilder(QObject* parent)
    : QObject(parent)
{
}

static QString ToQString(const std::string& s) { return QString::fromStdString(s); }

QWidget* SS_LightParamFormBuilder::Build(const BuildArgs& args, QWidget* parent)
{
    items_.clear();

    if (!args.tpl || !args.inst)
        return new QWidget(parent);

    // 滚动区域，避免参数多时撑爆
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);

    auto* container = new QWidget(scroll);
    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    // 遍历 params
    for (const auto& kv : args.tpl->params)
    {
        const SS_LightParamDef& def = kv.second;

        SS_LIGHT_PARAM_LOCATION loc = def.location;
        if (loc != args.location)
            continue;

        // label
        const std::string label = def.display_name.empty() ? def.key : def.display_name;

        // create widget
        SS_LightWidgetHandle handle = factory_.Create(def, container);

        // 设置初始值（用 inst 覆盖）
        const std::string init_val = GetInitialValue_(*args.inst, def, loc, args.channel_id);
        if (handle.set_value) handle.set_value(init_val);

        // 绑定变化 -> request
        Item item;
        item.instance_id = args.inst->info.instance_id;
        item.param_key = def.key;
        item.location = loc;
        item.channel_id = args.channel_id;
        item.handle = handle;

        if (handle.emitter)
        {
            QObject::connect(handle.emitter, &SS_LightWidgetChangeEmitter::SignalChanged, this, [this, item]() mutable
            {
                SS_LightParamSetRequest req;
                req.instance_id = item.instance_id;
                req.param_key = item.param_key;
                req.location = item.location;
                req.channel_id = item.channel_id;
                req.value_str = item.handle.get_value ? item.handle.get_value() : std::string{};
                emit SignalRequestReady(req);
            });
        }

        items_.push_back(item);
        form->addRow(ToQString(label), handle.widget);
    }

    root->addLayout(form);
    root->addStretch();

    container->setLayout(root);
    scroll->setWidget(container);
    return scroll;
}

void SS_LightParamFormBuilder::RefreshValues(const SS_LightControllerInstance& inst)
{
    for (auto& it : items_)
    {
        if (it.instance_id != inst.info.instance_id)
            continue;

        // 找 def：Refresh 只刷新值，不重新建控件，所以只能从 inst 读
        if (it.location == SS_LIGHT_PARAM_LOCATION::GLOBAL)
        {
            auto v = inst.global_param_values.find(it.param_key);
            if (v != inst.global_param_values.end() && it.handle.set_value)
                it.handle.set_value(v->second);
        }
        else if (it.location == SS_LIGHT_PARAM_LOCATION::CHANNEL)
        {
            auto v = inst.channel_param_values.find(it.param_key);
            if (v != inst.channel_param_values.end())
            {
                auto v2 = v->second.find(it.channel_id);
                if (v2 != v->second.end() && it.handle.set_value)
                    it.handle.set_value(v2->second);
            }
        }
    }
}

std::string SS_LightParamFormBuilder::GetInitialValue_(
    const SS_LightControllerInstance& inst,
    const SS_LightParamDef& def,
    SS_LIGHT_PARAM_LOCATION loc,
    const std::string& channel_id)
{
    // 优先用 instance 里的值
    if (loc == SS_LIGHT_PARAM_LOCATION::GLOBAL)
    {
        auto it = inst.global_param_values.find(def.key);
        if (it != inst.global_param_values.end())
            return it->second;
    }
    else if (loc == SS_LIGHT_PARAM_LOCATION::CHANNEL)
    {
        auto it = inst.channel_param_values.find(def.key);
        if (it != inst.channel_param_values.end())
        {
            auto it2 = it->second.find(channel_id);
            if (it2 != it->second.end())
                return it2->second;
        }
    }

    // fallback：template default
    if (!def.default_value.empty())
        return def.default_value;

    if (!def.widget.default_value.empty())
        return def.widget.default_value;

    return std::string{};
}
