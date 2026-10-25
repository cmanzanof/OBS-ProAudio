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

Primera corrida verde: Ubuntu 24.04, Ubuntu 26.04, Windows x64 y macOS arm64 compilan el fork y
publican un artefacto por plataforma. Los pushes que solo tocan documentación (``**.rst``,
``**.md``) no disparan builds.

Modelo de audio
---------------

Los *mixes* de libobs se usan como **buses de broadcast**:

* Mix ``0``: Master.
* Mixes ``1..7``: Bus A..G.

En el mezclador los buses se muestran como en vMix: ``M`` para el mix Master y ``A``..``G`` para
los buses auxiliares. Cada bus corresponde a una pista de OBS (el mix ``0`` es la pista 1), así
que las casillas de las Propiedades de Audio avanzadas son las de las pistas 1..6 de upstream más
las dos que agrega el fork.

Hoja de ruta
------------

1. **F1 - Motor de buses** (casi completo)

   1. ✔ ``MAX_AUDIO_MIXES`` 6 -> 8.
   2. ✔ Ganancia y mute por fuente y por bus (sends post-fader). Las rampas por bus quedan para
      F3, donde las necesita Audio Follow Video.
   3. ✔ Solo AFL/PFL no destructivo en la salida de monitoreo (auriculares), con volumen de
      auriculares propio.
   4. ✔ Medidores peak/RMS por bus.
   5. Pendiente: delay por entrada en milisegundos.

2. **F2 - UI del mezclador** (completa para el MVP)

   1. ✔ Buses ``M A B C D E F G`` por fuente y columna de solo en las Propiedades de Audio
      avanzadas, con persistencia de los envíos por bus.
   2. ✔ Botón Solo en cada tira del mezclador, con elección AFL/PFL, indicador de cuántas fuentes
      están en solo y "quitar todos los solos".
   3. ✔ Envíos por bus con faders, faders por bus, volumen de auriculares y medidores por bus.
      Pendiente: los faders de bus viven en una ventana propia; falta integrarlos como una franja
      fija arriba del mezclador, al estilo de la sección "Outputs" de vMix.
3. **F3 - Salidas y automatismos**: hasta 8 salidas a dispositivos, Audio Follow Video (AFV) y
   mix-minus.
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
* **F1.3**: ``libobs``: **Solo** (previsualización por auriculares) con modos AFL y PFL
  (``obs_source_set_solo_mode()`` / ``obs_source_get_solo_mode()`` / ``obs_source_solo_active()``,
  señal ``solo_mode``). Mientras haya al menos una fuente en solo, en el dispositivo de monitoreo
  solo se oyen las fuentes solistas, aunque su tipo de monitoreo sea "sin monitoreo". El solo
  **nunca** toca las mezclas: Master y los buses A..G siguen recibiendo exactamente el mismo audio,
  así que no puede alterar lo que se transmite o graba (igual que vMix). AFL usa el volumen
  post-fader (y, como en vMix, se oye aunque la fuente esté muteada); PFL escucha antes del fader.
  Sin cambios de comportamiento hasta que se active un solo.
* **F2.0**: ``libobs``: los envíos por bus (``bus_send``) ahora se guardan y cargan con la fuente,
  en una clave ``bus_sends`` del archivo de escena. Solo se escribe cuando algún bus se movió del
  valor unitario, así que las colecciones de escenas siguen siendo idénticas a las de upstream si
  no se usó el mezclador, y OBS original simplemente ignora la clave.
* **F2.1**: ``frontend``: Propiedades de Audio avanzadas. Las casillas de pistas ya no están
  hardcodeadas a las 6 de upstream: se generan a partir de ``MAX_AUDIO_MIXES`` y se rotulan como
  en vMix (``M`` para Master, ``A``..``G`` para los buses), con el nombre de la mezcla en el
  tooltip y el encabezado "Buses". Se agrega una columna **Solo** (desactivado / AFL / PFL) por
  fuente, que se mantiene sincronizada con la señal ``solo_mode``.
* **F2.2**: ``frontend``: cada tira del mezclador tiene un botón **S** de solo (clic derecho para
  elegir AFL o PFL, AFL por defecto). La barra del mezclador muestra un indicador con la cantidad
  de fuentes en solo y permite quitarlas todas de una vez, para que un solo olvidado no pase
  desapercibido.
* **F2.3**: ``frontend``: los envíos por bus se ajustan desde una ventana con un fader por bus
  (de silencio a +6 dB, en dB como en vMix), que se abre desde la fila de la fuente en las
  Propiedades de Audio avanzadas o desde el menú contextual de su tira en el mezclador. Todo el
  diálogo es un solo paso de deshacer. De paso se arreglaron las señales ``bus_send`` y
  ``solo_mode``: estaban emitidas sin declarar, así que ningún ``connect`` las recibía y la UI no
  se enteraba de los cambios hechos por otra vía.
* **F2.4**: ``libobs``: ganancia maestra por bus (``obs_audio_set_bus_volume()`` /
  ``obs_audio_get_bus_volume()``), aplicada en el momento de sumar las fuentes al mix, después del
  volumen de cada fuente y de los envíos, y ganancia del dispositivo de monitoreo
  (``obs_audio_set_monitoring_volume()`` / ``obs_audio_get_monitoring_volume()``), que se aplica
  por encima de lo que esté sonando en los auriculares. ``frontend``: la barra del mezclador abre
  una ventana con los faders de ``M`` y ``A``..``G`` más el volumen de auriculares, con un botón
  para volver todo a 0 dB. La ventana no es modal, así que se puede mezclar con ella abierta, y
  todo lo que se toca en ella es un solo paso de deshacer.
* **F1.4**: ``libobs``: ``obs_volmeter_attach_bus()``, que engancha un medidor de nivel a un bus
  en lugar de a una fuente, usando el mismo mix que alimenta sus salidas (o sea, mide después del
  volumen de cada fuente, de los envíos y del fader del bus). ``frontend``: la ventana de buses
  muestra un medidor compacto por bus, con RMS, pico y retención de pico.
* **CI**: primer arranque del workflow del fork. Se publican los tags de upstream en ``origin``
  (sin ellos ``versionconfig.cmake`` rompe el configure), el job de macOS pasa a ``macos-26`` con
  Xcode 26.5, el de Windows a ``windows-2025-vs2026`` y Ubuntu compila contra 24.04 y 26.04. Los
  jobs que fallan vuelcan el final de su log como anotaciones y los pushes de solo-docs no
  disparan builds. Corridas verdes: F1.1, F1.2, F1.3, F2.0 a F2.4, y el motor de F1.4, en las
  cuatro plataformas.

Licencia
--------

OBS Studio es GPL-2.0-or-later; este fork se distribuye bajo la misma licencia y publica su
código fuente. No es un producto oficial del proyecto OBS.
