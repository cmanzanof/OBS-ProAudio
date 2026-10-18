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

CI
--

``.github/workflows/proaudio-ci.yaml`` compila el fork en Ubuntu, Windows y macOS y publica
artefactos. Los workflows de upstream no se disparan en el fork porque están limitados a las
ramas ``master``/``release/**``. El plugin de navegador (CEF) queda deshabilitado: es ortogonal
al audio y evita una descarga de ~1 GB.

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

Licencia
--------

OBS Studio es GPL-2.0-or-later; este fork se distribuye bajo la misma licencia y publica su
código fuente. No es un producto oficial del proyecto OBS.
