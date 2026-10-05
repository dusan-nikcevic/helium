import QtQuick

Item {
    id: root
    property real levelL: .5
    property real levelR: .5
    property int meterWidth: 7
    property bool showScale: false
    implicitWidth: meterWidth * 2 + 5 + (showScale ? 20 : 0)
    implicitHeight: 160
    Accessible.role: Accessible.Indicator
    Accessible.name: "Stereo meter, demo levels"
    Row {
        anchors.fill: parent; spacing: 3
        Repeater {
            model: [root.levelL, root.levelR]
            Rectangle {
                required property real modelData
                width: root.meterWidth; height: root.height; radius: 2; color: "#070709"; clip: true
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.danger }
                        GradientStop { position: .26; color: Theme.warning }
                        GradientStop { position: .52; color: Theme.success }
                        GradientStop { position: 1; color: "#2fa05f" }
                    }
                }
                Rectangle { width: parent.width; height: parent.height * (1 - Math.max(0, Math.min(1, parent.modelData))); color: "#ee09090b" }
                Repeater { model: 18; Rectangle { required property int index; width: root.meterWidth; height: 1; y: (index + 1) * root.height / 18; color: "#a008080a" } }
                Rectangle { width: parent.width; height: 2; y: Math.max(0, parent.height * (1 - parent.modelData) - 4); color: "#c5c5ca" }
            }
        }
        Item {
            width: 18; height: root.height; visible: root.showScale
            Repeater {
                model: ["+6", "0", "−6", "−12", "−24", "−∞"]
                Text { required property string modelData; required property int index; text: modelData; y: index * (root.height - 9) / 5; color: Theme.faint; font.family: Theme.monoFamily; font.pixelSize: 7 }
            }
        }
    }
}
