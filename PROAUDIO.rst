ProAudio Studio
===============

Fork **no oficial** de `OBS Studio <https://github.com/obsproject/obs-studio>`__ cuyo objetivo es
llevar la sección de audio a la paridad funcional con vMix: buses con envío por fuente, Solo
AFL/PFL, múltiples salidas a dispositivos, Audio Follow Video y soporte VST3.

Ramas
-----

* ``stable-32``: espejo del tag estable de upstream en que se basa el fork (32.2.2). No se
  commitea aquí: solo se mergea hacia ``main``.
* ``main``: integración del fork.
* ``feat/*``: un tema por rama, con commits atómicos (candidatos a PR upstream).

Nombre
------

El producto se llama **ProAudio Studio**. El repositorio git se llama ``OBS-ProAudio`` por
comodidad (es un fork de OBS Studio), pero el binario, el instalador, el título de la ventana y
el README usan **ProAudio Studio** y aclaran que es un fork no oficial: el proyecto OBS tiene su
propia política de marca y no queremos sugerir respaldo oficial.

Remotes
-------

* ``upstream``: ``https://github.com/obsproject/obs-studio``
* ``origin``: ``https://github.com/cmanzanof/OBS-ProAudio``

Puesta en marcha del remoto
---------------------------

.. code:: bash

   git remote add origin https://github.com/cmanzanof/OBS-ProAudio.git
   git push -u origin main
   git push -u origin feat/buses-core

   # Los tags son obligatorios, no decorativos (ver CI).
   git push origin 32.2.2

CI
--

``.github/workflows/proaudio-ci.yaml`` compila el fork en Ubuntu, Windows y macOS y publica
artefactos. Los workflows de upstream no se disparan en el fork porque están limitados a las
ramas ``master``/``release/**``. El plugin de navegador (CEF) queda deshabilitado: es ortogonal
al audio y evita una descarga de ~1 GB.

Tres cosas que el workflow tiene que respetar (nos costaron la primera corrida):

* **Los tags deben estar publicados en ``origin``.** ``cmake/common/versionconfig.cmake`` deduce
  la versión con ``git describe --tags``; sin tags devuelve un hash suelto y
  ``project(... VERSION <hash>)`` aborta a los pocos segundos de configurar, en las tres
  plataformas. Por eso el checkout usa ``fetch-tags: true``.
* **macOS** exige el SDK 26.5 (``cmake/macos/compilerconfig.cmake``), o sea Xcode 26.5: el job
  corre en ``macos-26`` y selecciona ese Xcode a mano.
* **Windows** usa el preset ``windows-x64``, que pide el generador *Visual Studio 18 2026* y el
  SDK 10.0.26100: el job corre en ``windows-2025-vs2026`` (``windows-2022`` solo trae VS 2022).

Además, cada job que falla vuelca el final de su log como anotaciones ``::error::``: el API de
logs de jobs requiere permisos de administración, pero las anotaciones se leen sin
autenticación.

Modelo de audio
---------------

Los *mixes* de libobs se usan como **buses de broadcast**:

* Mix ``0``: Master.
* Mixes ``1..7``: Bus A..G.

Hoja de ruta
------------

1. **F1 - Motor de buses** (en curso)

   1. ``MAX_AUDIO_MIXES`` 6 -> 8.
   2. Ganancia y mute por fuente y por bus (sends post-fader). Las rampas por bus quedan para
      F3, donde las necesita Audio Follow Video.
   3. Solo AFL/PFL no destructivo y bus de monitoreo.
   4. Medidores peak/RMS por bus.
   5. Delay por entrada en milisegundos.

2. **F2 - UI del mezclador**: botones ``M A B C D E F G`` por fuente, envíos por bus en las
   propiedades avanzadas, medidores pre/post y vista de buses.
3. **F3 - Salidas y automatismos**: hasta 8 salidas a dispositivos, bus de headphones,
   Audio Follow Video (AFV) y mix-minus.
4. **F4 - Plugins**: host VST3 y matriz de canales.

Registro de cambios propios
---------------------------

* **F1.1**: ``libobs``: ``MAX_AUDIO_MIXES`` 6 -> 8 (cada mix pasa a ser un bus). ``frontend``:
  ``GetAudioBitrate()`` ya no asume exactamente 6 pistas (antes leía fuera de un array de 6 al
  llegar al mix 7) y el selector de pista VOD queda acotado a las 6 pistas que la UI expone hoy.
  Sin cambios de comportamiento visibles.
* **F1.2**: ``libobs``: ganancia de envío por fuente y por bus
  (``obs_source_set_bus_send()`` / ``obs_source_get_bus_send()``), aplicada post-fader en la
  ruta de render, con señal ``bus_send`` para la UI. Por defecto vale 1.0, así que la mezcla
  existente suena exactamente igual que antes.
* **CI**: primer arranque del workflow del fork. Se publican los tags de upstream en ``origin``
  (sin ellos ``versionconfig.cmake`` rompe el configure), el job de macOS pasa a ``macos-26`` con
  Xcode 26.5, el de Windows a ``windows-2025-vs2026`` y Ubuntu compila contra 24.04 y 26.04. Los
  jobs que fallan vuelcan el final de su log como anotaciones.

Licencia
--------

OBS Studio es GPL-2.0-or-later; este fork se distribuye bajo la misma licencia y publica su
código fuente. No es un producto oficial del proyecto OBS.
