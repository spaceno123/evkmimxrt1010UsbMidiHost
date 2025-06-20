# evkmimxrt1010UsbMidiHost
  
SDK の evkmimxrt1010_host_msd_fatfs_freertos をベースに DebugMonitor を追加して、  
さらに evkmimxrt1010_host_hid_mouse_keyboard_freertos の keyboard 部分を追加して、  
USB Host Class に MIDI を追加してみました。  

**USB HOST MIDI に関する追加は以下の通り**  

- source に以下のファイルを追加  
host_midi.c  
host_midi.h  
- usb>host>class に以下のファイルを追加  
usb_host_midi.c  
usb_host_midi.h  
- source の usb_host_config.h に以下を追加
```
#define USB_HOST_CONFIG_MIDI (1U)
```
- app.c に必要な処理を追加

**テスト動作について**

```
接続構成

[pc serial console]---(Debug USB)---[evkmimxrt1010]---(USB OTG)---[HUB]---*---[usb keboard]  
 ___ ___     ___ ___     ___ ___ ___                                      |
|   |   |   |   |   |   |   |   |   |                                     *---[usb memory]
| A | S |   | F | G |   | J | K | L |                                     |
|___|___|___|___|___|___|___|___|___|_ ___                                *---[midi device]
  |   |   |   |   |   |   |   |   |   |   |
  | Z | X | C | V | B | N | M | , | . | / |   send note on/off keys
  |___|___|___|___|___|___|___|___|___|___|
```
- pc serial console に midi device からのメッセージがダンプ表示されます。  
  (リアルタイムメッセージと Debug Monitor 移行および起動状態では表示しません。)
- pc serial console と usb keyboard の入力は、'C'キーを note 番号60として押下時に note on & off を midi device へ送ります。  
  (Debug Monitor 移行および起動状態では送られません。)
- pc serial console または usb keyboard で '@' を3回入力すると Debug Monitor が起動します。  
  Debug Monitor 起動後、Dir 1:(リターン) とすることで、usb memory のルートディレクトリが表示できます。  
  '@'を入力すると、Debug Monitor を終了します。
  
