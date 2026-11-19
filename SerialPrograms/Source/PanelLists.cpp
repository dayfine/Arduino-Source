/*  Left-Side Panel
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <QStandardItemModel>
#include <QVBoxLayout>
#include "Common/Qt/NoWheelComboBox.h"
#include "CommonFramework/PersistentSettings.h"
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/Windows/DpiScaler.h"
#include "CommonFramework/Panels/UI/PanelListWidget.h"
#include "PanelLists.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{



ProgramSelect::ProgramSelect(QWidget& parent, PanelHolder& holder)
    : QWidget(&parent)
    , m_holder(holder)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignTop);
    m_dropdown = new NoWheelCompactComboBox(this);
    m_dropdown->setMaxVisibleItems(20);
    this->setStyleSheet("QComboBox QAbstractItemView::item { min-height: 24px; }");
    {
        QListView* list_view = new QListView(this);
        list_view->setWordWrap(true);
        m_dropdown->setView(list_view);
    }
    {
//        QSize size = m_dropdown->iconSize();
//        cout << "size = " << size.width() << " x " << size.height() << endl;
//        size.setWidth(size.width() * 2);
        m_dropdown->setIconSize(QSize(48, 24));
    }
    layout->addWidget(m_dropdown);


    const auto& lists = m_catalog.lists();
    for (size_t c = 0; c < lists.size(); c++){
        add_dropdown_entry(*lists[c], static_cast<int>(c));
    }

    //  Load the 1st list by default.
    m_dropdown->setCurrentIndex(0);
    m_active_index = 0;
    m_active_list = lists[0]->make_QWidget(*this, m_holder);
    layout->addWidget(m_active_list);

    connect(
        m_dropdown, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
        this, [this](int index){
            change_list(index);
        }
    );
}

void ProgramSelect::lock(){
    m_locked = true;
    m_active_list->setEnabled(false);
}
void ProgramSelect::unlock(){
    m_locked = false;
    m_active_list->setEnabled(true);
}

void ProgramSelect::add_dropdown_entry(const PanelListDescriptor& list, int index){
    const ImageViewRGB32& icon = list.icon();
    if (icon){
        m_dropdown->addItem(
            QIcon(QPixmap::fromImage(to_QImage_ref(icon))),
            QString::fromStdString(list.name())
        );
    }else{
        m_dropdown->addItem(QString::fromStdString(list.name()));
    }

    if (!list.enabled()){
        auto* model = qobject_cast<QStandardItemModel*>(m_dropdown->model());
        if (model != nullptr){
            QStandardItem* line_handle = model->item(index);
            if (line_handle != nullptr){
                line_handle->setEnabled(false);
            }
        }
    }
}



void ProgramSelect::load_persistent_panel(){
    const std::string* str = PERSISTENT_SETTINGS().panels.get_string("ProgramCategory");
    if (str == nullptr){
        return;
    }
    const std::optional<size_t> category = m_catalog.find_category(*str);
    if (!category){
        return;
    }
    const int index = static_cast<int>(*category);
    m_dropdown->setCurrentIndex(index);
    change_list(index);
    str = PERSISTENT_SETTINGS().panels.get_string(PanelListWidget::JSON_PROGRAM_PANEL);
    if (str != nullptr){
        m_active_list->set_panel(*str);
    }
}

std::string ProgramSelect::select_program(const std::string& identifier){
    std::optional<PanelCatalogMatch> match = m_catalog.find_panel(identifier);
    if (!match){
        return "No program with identifier \"" + identifier + "\".";
    }
    if (m_locked){
        return "A program is running.";
    }
    change_list(static_cast<int>(match->category_index));
    m_active_list->set_panel(match->display_name);
    return "";
}

void ProgramSelect::change_list(int index){
    if (m_active_index == index && m_active_list != nullptr){
        return;
    }
    const auto& lists = m_catalog.lists();
    m_dropdown->setCurrentIndex(index);
    m_active_index = index;
    PERSISTENT_SETTINGS().panels["ProgramCategory"] = lists[index]->name();
    delete m_active_list;
    m_active_list = lists[index]->make_QWidget(*this, m_holder);
    m_active_list->setEnabled(!m_locked);
    layout()->addWidget(m_active_list);
}

QSize ProgramSelect::sizeHint() const{
    QSize size = QWidget::sizeHint();
//    cout << size.width() << " x " << size.height() << endl;
//    cout << this->size().width() << " x " << this->size().height() << endl;
    size.setWidth(scale_dpi_width(size.width() + 10));
    return size;
}







}
