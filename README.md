# Yandex disk plugin for Total/Double Commander

WFX плагин для файлового менеджера Double Commander, позволяющий работать с сервисом Yandex Disk через Web API.

В настоящий момент плагин работает в *nix системах. WIN32 версия будет реализована в скором времени.
Реализована работа только с одним аккаунтом, возможность скачивания файлов из интернета на яндекс диск, работа с корзиной.

### Установка

Скачать [последний релиз](https://github.com/ivanenko/ydisk_commander/releases/)

Вы можете самостоятельно собрать из исходников плагин. Для этого у вас должны быть установленa программа cmake.
В папке со скачанным кодом запустите команду
```
cmake --build cmake-build-release --target ydisk_commander -- -j 2 -DCMAKE_BUILD_TYPE=Release
```
В папке cmake-build-release возьмите файл ydisk_commander.wfx и установите его как плагин для Double Commander.

### Настройка и использование

После установки плагина в сетевом окружении у вас появится Yandex Disk. Нажав на него правой кнопкой мыши, можно вызвать диалоговое окно с настройками авторизации.
По умолчанию используется двухфакторная авторизация, и полученный токен не хранится нигде. Можно же настроить его сохранение в конфигурационном файле, или в менеджере паролей.

#### Скачивание файлов

Yandex Disk позволяет скачивать файлы из интернета. В плагине так же реализована эта возможность. Для этого зайдите в требуемую папку диска, и наберите комманду
```
download FILE_URL [new_file_name]
```

FILE_URL - ссылка на скачиваемый файл, new_file_name - опциональный параметр, позволяющий переименовать файл при сохранении.

#### Очистка корзины

Для полной очистки корзины диска в коммандной строке файлового менеджера введите команду
```
trash clean
```

Если же вы хотите удалить конкретный файл из корзины, вам нужно зайти в папку /.Trash, выбрать нужный файл, и нажать Del (F8)