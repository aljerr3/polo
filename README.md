# 🪲 POLO — Escarabajo Robot Caminante Open Source

<p align="center">
  <img src="images/polo.jpg" alt="POLO — Escarabajo Robot Caminante" width="600">
</p>

¿Se puede hacer caminar un robot de seis patas con solo tres motores? **POLO** es un escarabajo robot **100% imprimible**, inspirado en el *pololo* —un escarabajo chileno— y en los caminantes minimalistas de **Mark Tilden**, a quien este proyecto rinde tributo.

Este no es solo un proyecto de electrónica. Es un ejercicio de mecánica: rótulas impresas en una sola pieza, acoples lobulares, bielas y un balancín que, juntos, hacen que seis patas se muevan de forma fluida y casi orgánica.

---

## 🎯 ¿Qué hace este robot?

- Camina hacia adelante, hacia atrás y gira usando **solo 3 servos**
- Se controla desde el celular **por WiFi, sin apps**, directo desde el navegador
- Tiene **4 velocidades de marcha**: muy rápido, rápido, lento y sigiloso
- Sus ojos pulsan simulando el latido del corazón de un escarabajo
- Emite sonidos de **estridulación** con un buzzer, igual que los escarabajos reales
- Se carga por **USB-C** sin quitar el caparazón

---

## ⚙️ Componentes principales

| Componente | Detalle |
|-----------|---------|
| Microcontrolador | ESP32-C3 Super Mini |
| Actuadores | 3 servos SG90 |
| Batería | LiPo 803040 de 1000 mAh (3,7 V) |
| Gestión de energía | Módulo PB0063A (IP5306): carga, protección y elevador a 5 V |
| Filtrado | Condensador electrolítico de 470 µF |
| Ojos | 2 LEDs de 5 mm |
| Sonido | Buzzer pasivo |
| PCB | POLO PCB (fabricada por PCBWay) o perfboard armada a mano |
| Estructura | Piezas impresas en 3D (PLA) |

---

## 🧠 Filosofía del proyecto

Hoy la electrónica y el software están al alcance de cualquiera, sobre todo con la ayuda de la IA. La mecánica, en cambio, sigue sorprendiendo. Por eso el corazón de POLO no está en el código, sino en cómo se mueven sus piezas.

El robot se inspira en **BEAM**, la filosofía de Mark Tilden: lograr comportamientos complejos con el mínimo de componentes. Un servo mueve las dos patas del lado izquierdo, otro las del lado derecho, y un tercero acciona un balancín central que inclina el cuerpo para levantar las patas del lado contrario.

POLO es también un proyecto de aprendizaje. Un caminante así se puede hacer con 2 servos, y probablemente mejor. Este es mi camino para llegar a él, y lo comparto completo para que otros lo mejoren.

---

## 🔩 Características mecánicas

- **100% imprimible**: todas las piezas estructurales son impresas en 3D
- Rótulas articuladas impresas en sitio, en una sola pieza y **sin soportes**: el tobillo cae unos 45° al levantar la pata y se autonivela al apoyar
- Pies de suela plana alargada para mejor tracción
- Acoples lobulares indexados de 10 lóbulos (pasos de 36°) para ajustar la posición de cada tibia
- Bielas mecánicas y balancín central movidos por los mismos 3 servos
- Optimización topológica para reducir peso, material y consumo
- Caparazón tipo élitro atornillado al chasis, con aberturas para los puertos USB-C
- Casco con cachos a presión, que hacen las veces de antenas
- Tamaño: unos **12,6 cm** de punta a punta de patas

---

## 📡 Características técnicas

- **WiFi en modo punto de acceso**: el robot crea su propia red, sin necesidad de router
- Interfaz web responsive con dos vistas: **manejo** y **configuración**
- Control de baja latencia: mantén presionada una flecha para caminar y suelta para volver a reposo
- Calibración de servos y rutinas de marcha (ángulos y velocidad) desde el navegador
- Valores por defecto medidos con osciloscopio, listos para quien no pueda calibrar
- Control independiente de cada ojo con atenuación por PWM
- Puerto **I2C (Qwiic)** libre para agregar tus propios sensores
- Consumo medido: **~0,36 A a 5 V (1,75 W)**

---

## 🔌 Conexiones eléctricas

**ESP32-C3 Super Mini:**

| GPIO | Función |
|------|---------|
| GPIO 4 | Servo pata izquierda |
| GPIO 3 | Servo pata derecha |
| GPIO 2 | Servo balancín central |
| GPIO 10 | LED ojo 1 (cátodo; ánodo a 5 V con resistencia) |
| GPIO 1 | LED ojo 2 (cátodo; ánodo a 5 V con resistencia) |
| GPIO 7 | Buzzer (señal S) |
| GPIO 5 | SDA (puerto Qwiic) |
| GPIO 6 | SCL (puerto Qwiic) |

**Alimentación:** Batería LiPo → módulo IP5306 → riel de 5 V. El ESP32 y los 3 servos comparten el riel de 5 V, con un condensador de 470 µF.

Ver el esquemático completo en [`hardware/Esquematico.png`](hardware/Esquematico.png).

---

## 🛠️ Fabrica tu propia PCB con PCBWay

La placa de POLO está diseñada para encajar exactamente en el chasis. Es de 2 capas y mide **50,1 x 40,3 mm**, con contorno recortado a la forma del robot. Lleva máscara negra y serigrafía blanca, componentes through-hole fáciles de soldar, planos de masa en ambas caras y dos puertos USB-C (carga y programación).

Gracias a **PCBWay**, patrocinador de la PCB de este proyecto, puedes pedir la misma placa directamente desde su plataforma. Los archivos Gerber están en [`hardware/Gerber_POLO_PCB.zip`](hardware/Gerber_POLO_PCB.zip).

Si prefieres armarla a mano, el primer prototipo de POLO funcionó perfectamente sobre **perfboard**.

---

## 🚀 Instalación y configuración

1. Imprime las piezas (recomendado: PLA, capa 0,12 mm; las tibias con rótula se imprimen con la cara plana sobre la cama, sin soportes)
2. Suelda la PCB o arma el circuito en perfboard según el esquema
3. Carga el firmware ([`firmware/POLO_v7_1.ino`](firmware/POLO_v7_1.ino)) en el ESP32-C3 por el puerto USB-C de programación
4. Monta los servos en su posición de reposo antes de fijar las patas
5. Enciende POLO y conéctate desde tu celular a su red WiFi
6. Abre la IP del robot en el navegador
7. En la vista de configuración, ajusta el reposo y la marcha si lo necesitas
8. ¡Pasa a la vista de manejo y hazlo caminar!

### ⚙️ Configuración personalizable (desde la web)

- Posición de reposo de cada servo (por defecto **60° / 111° / 68°**)
- Ángulos de oscilación para avanzar, retroceder y girar
- Periodo de la marcha (por defecto **700 ms**)
- Velocidades de marcha
- Encendido y brillo de los ojos

---

## 📁 Contenido del repositorio

```
polo/
├── firmware/
│   └── POLO_v7_1.ino          # Firmware para el ESP32-C3
├── hardware/
│   ├── Esquematico.png         # Esquemático del circuito
│   ├── PCB_Diseno.png          # Diseño de la PCB
│   └── Gerber_POLO_PCB.zip     # Archivos Gerber para fabricación
└── images/                     # Fotos del robot
```

> **Nota:** el diseño 3D en Fusion 360 y los STL se añadirán próximamente.

---

## 🧰 Tecnologías

ESP32-C3 · Servos SG90 · Impresión 3D · Fusion 360 · EasyEDA · PCBWay · WiFi AP · Servidor web embebido · PWM · I2C

---

## 📜 Licencia

Este proyecto está bajo la licencia **MIT**. Puedes modificarlo, adaptarlo y mejorarlo. Ver [LICENSE](LICENSE).

---

Hecho por **Alejandro Jesús Rodríguez Radomile** · [rodriguezalejandro.com](https://rodriguezalejandro.com)
