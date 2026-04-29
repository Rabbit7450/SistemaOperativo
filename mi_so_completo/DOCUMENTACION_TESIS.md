# MI SO v3.3
## Documento técnico integral estilo tesis

**Autor del proyecto:** Adalit  
**Línea de trabajo:** Sistemas Operativos - Núcleo educativo x86 (32-bit)  
**Versión documentada:** v3.3  
**Plataforma objetivo:** i386 compatible, ejecución en QEMU  

---

## Resumen

Este documento presenta el diseño, implementación y estado actual de `MI SO v3.3`, un sistema operativo educativo de arquitectura modular para x86 en modo protegido de 32 bits. El sistema implementa una ruta de arranque desde BIOS, transición a modo protegido, inicialización de subsistemas de núcleo, manejo de interrupciones por IDT/PIC, ejecución de procesos con planificador por prioridades, soporte de llamadas al sistema vía `INT 0x80`, consola de texto sobre memoria VGA y una shell interactiva con comandos de gestión de procesos y servicios simulados.

El proyecto está orientado al aprendizaje de fundamentos de sistemas operativos, con especial énfasis en bajo nivel (arranque, privilegios, interrupciones, scheduler y separación kernel/usuario).

---

## 1. Introducción

### 1.1 Contexto

El desarrollo de un sistema operativo mínimo permite integrar conocimientos de arquitectura de computadores, programación en C freestanding, ensamblador x86, gestión de memoria y concurrencia básica. `MI SO` se concibe como una plataforma didáctica con componentes incrementales y verificables.

### 1.2 Objetivo general

Construir un núcleo modular ejecutable en entorno virtualizado que permita:

- Inicializar hardware básico en x86.
- Gestionar procesos y tiempos de CPU.
- Atender interrupciones y llamadas al sistema.
- Exponer una interfaz de usuario por shell.
- Simular gestión de servicios y operaciones de archivos con control por roles.

### 1.3 Alcance

Incluye:

- Bootloader de 512 bytes.
- Modo protegido 32-bit.
- Tabla de descriptores globales (segmentación kernel/usuario).
- IDT con excepciones, timer, teclado y syscall.
- Planificador por prioridad con envejecimiento.
- Procesos con estados y contexto.
- Shell modular con comandos administrativos.

No incluye:

- Paginación real.
- Sistema de archivos persistente en disco real.
- Controladores avanzados (ATA, red, USB).
- Multiprocesamiento simétrico (SMP).

---

## 2. Metodología de desarrollo

### 2.1 Enfoque

Desarrollo incremental por capas:

1. Arranque y transición de CPU.
2. Consola y depuración visual.
3. Interrupciones base.
4. Gestión de procesos.
5. Interfaz shell.
6. Extensiones de servicios y seguridad por roles.

### 2.2 Entorno

- Ensamblador: NASM
- Compilador C: GCC (`-m32`, `-ffreestanding`)
- Enlazador: GNU LD (`elf_i386`)
- Emulación: QEMU (`qemu-system-i386`)
- Automatización: Make

---

## 3. Arquitectura general del sistema

### 3.1 Vista de módulos

El proyecto se organiza en:

- `boot.asm`: carga inicial y salto al kernel.
- `linker.ld`: mapeo de secciones del kernel en memoria.
- `src/kernel.c`: inicialización global y lazo principal.
- `src/interrupts.c`: IDT, PIC, IRQ, excepciones y syscall.
- `src/process.c`: tabla de procesos y planificación.
- `src/shell.c`: parser/dispatcher de comandos.
- `src/servers.c`: simulación de servicios, auditoría y FS lógico.
- `src/vga.c`: salida en texto sobre `0xB8000`.
- `src/utils.c`: utilidades de cadena y parsing.
- `include/*.h`: contratos de cada subsistema.

### 3.2 Flujo de ejecución

1. BIOS carga sector de arranque en `0x7C00`.
2. Bootloader configura segmentos, carga kernel desde disco, activa modo protegido.
3. Salto a `kernel_main`.
4. Kernel inicializa procesos, servicios, FS simulado e IDT.
5. Se habilitan interrupciones.
6. Shell queda en espera de comandos de usuario.

---

## 4. Proceso de arranque

### 4.1 Bootloader

El bootloader implementa:

- Inicialización de registros de segmento y stack temporal.
- Mensaje de estado por BIOS (`int 0x10`).
- Lectura del kernel con `int 0x13`.
- Conservación de unidad de arranque desde `DL`.
- Reintentos de lectura ante fallo.
- Carga de GDT y activación de bit PE en `CR0`.
- Salto lejano a código 32-bit.

### 4.2 Firma de arranque

El sector finaliza con:

- Relleno hasta byte 510.
- Firma `0xAA55`.

Esto garantiza compatibilidad de arranque BIOS clásico.

---

## 5. Núcleo y organización de memoria

### 5.1 Punto de entrada del kernel

`kernel_main()`:

- Limpia pantalla.
- Presenta banner de versión.
- Inicializa subsistemas.
- Entra en ciclo interactivo de shell.

### 5.2 Distribución de memoria (actual)

- `0x00007C00`: bootloader
- `0x00001000`: imagen del kernel (secciones enlazadas)
- `0x000B8000`: memoria VGA texto
- `0x00090000`: stack inicial del kernel
- `0x00010000+`: bloques lógicos para procesos de usuario

### 5.3 Segmentación y privilegios

GDT con descriptores:

- Kernel code/data (Ring 0).
- User code/data (Ring 3).

Se soporta transición a modo usuario y retorno por interrupciones.

---

## 6. Sistema de interrupciones

### 6.1 IDT

Se inicializan 256 entradas de IDT y se cargan con `lidt`.

Entradas relevantes:

- `0x00-0x1F`: excepciones CPU.
- `0x20`: IRQ0 (timer).
- `0x21`: IRQ1 (teclado).
- `0x80`: syscall gate con privilegio de usuario.

### 6.2 PIC

Se remapea el PIC para evitar colisiones con excepciones de CPU y se habilitan IRQ necesarias.

### 6.3 Excepciones

Existe manejador para excepciones con mensaje y detención segura del sistema (`cli; hlt`).

---

## 7. Gestión de procesos y planificación

### 7.1 Estructura de proceso

Cada `Process` contiene:

- PID, nombre.
- Estado (`RUNNING`, `READY`, `SLEEP`, `ZOMBIE`, etc.).
- Prioridad.
- Rango lógico de memoria.
- Modo de ejecución (`KERNEL_MODE` / `USER_MODE`).
- Contexto de registros de usuario.
- Contadores de sueño, uso de CPU y espera.

### 7.2 Estados

- `PROC_EMPTY`: slot libre.
- `PROC_RUNNING`: en ejecución.
- `PROC_READY`: listo para ejecutar.
- `PROC_SLEEP`: suspendido por ticks.
- `PROC_ZOMBIE`: finalizado pendiente de limpieza.

### 7.3 Algoritmo de planificación

El scheduler opera sobre ticks de timer:

- Selección por mejor prioridad.
- Empate resuelto por rotación circular.
- Envejecimiento de procesos en espera para evitar inanición.
- Cuantización temporal con cambio periódico.

### 7.4 Ciclo de vida

- Creación por `exec`.
- Activación a `RUNNING` por scheduler.
- Suspensión por `sleep`.
- Terminación por `kill` o `exit`.
- Liberación de slot en limpieza de zombies.

---

## 8. Modo usuario y llamadas al sistema

### 8.1 Entrada a usuario

El kernel permite ejecutar procesos en segmento de usuario mediante `iret` hacia selector Ring 3 y stack de usuario configurado.

### 8.2 ABI de syscall (`INT 0x80`)

Canal de entrada:

- `EAX`: número de syscall.
- `EBX`, `ECX`: argumentos principales.

Retorno:

- `EAX`: resultado o error (`-1`).

### 8.3 Syscalls implementadas

- `SYS_EXIT`
- `SYS_GETPID`
- `SYS_GETMEM`
- `SYS_SLEEP`
- `SYS_SETPRIO`
- `SYS_GETPRIO`
- `SYS_WRITE`

### 8.4 Seguridad de argumentos

La syscall de escritura incorpora validación de rango de punteros de usuario antes de acceder a memoria, evitando lectura fuera del espacio asignado al proceso actual.

---

## 9. Subsistema de entrada/salida

### 9.1 Consola VGA

Driver en modo texto:

- Escritura de caracteres y cadenas.
- Soporte de salto de línea y retroceso.
- Scroll de pantalla.
- Limpieza total.

### 9.2 Teclado

Manejo por IRQ1:

- Traducción scancode -> ASCII básico.
- Buffer circular de teclado.
- Consumo por shell.
- Espera de entrada con pausa del CPU en lugar de polling continuo.

---

## 10. Shell del sistema

### 10.1 Modelo

La shell es un intérprete de línea de comandos:

- Captura de entrada.
- Parseo de tokens.
- Ejecución por tabla condicional de comandos.

### 10.2 Comandos principales

Sistema:

- `help`, `about`, `clear`, `uptime`, `memory`, `cmd`, `principales`, `whoami`

Sesión:

- `login <rol> <clave>`, `logout`

Procesos:

- `ps`, `top`, `exec`, `runu`, `kill`, `sleep`, `priority`

Utilidades:

- `add`, `echo`

Servicios:

- `server list|status|start|stop|restart|backup|restore|audit`
- `server fs dirs|mkdir|touch|ls|write|cat|rm|rmdir|chmod`

---

## 11. Gestión de servicios simulados

### 11.1 Modelo de servidor

Cada servicio simulado mantiene:

- Nombre.
- Estado (`UP/DOWN`).
- Carga CPU.
- Memoria.
- Salud.
- Reinicios acumulados.

### 11.2 Control de acceso por rol

Roles:

- `viewer`
- `operator`
- `admin`

Política:

- Operaciones críticas restringidas por rol mínimo.
- Sesión requerida para acciones administrativas.
- Mecanismo de autenticación y cierre de sesión.

### 11.3 Auditoría

Registro circular de operaciones administrativas y de archivos.

El log guarda eventos en orden cronológico reciente para revisión con `server audit`.

---

## 12. Sistema de archivos simulado por servidor

### 12.1 Diseño

Cada servidor dispone de:

- Tabla de directorios (`MAX_SERVER_DIRS`).
- Tabla de archivos (`MAX_SERVER_FILES`).
- Snapshot de respaldo/restauración.

### 12.2 Operaciones soportadas

- Enumeración de directorios.
- Creación/eliminación de directorios.
- Creación/eliminación de archivos.
- Escritura/lectura de contenido.
- Ajuste de permisos (`r`, `w`, `rw`).
- Backup/restore por servidor.

### 12.3 Restricciones

- Sin persistencia real en disco.
- Límites estáticos de capacidad.
- Tamaño de contenido acotado por buffer.

---

## 13. Compilación, ejecución y depuración

### 13.1 Flujo de compilación

1. `nasm` genera `boot.bin`.
2. `gcc` compila objetos del kernel.
3. `ld` enlaza `kernel.bin` con `linker.ld`.
4. Concatenación `boot.bin + kernel.bin => os-image.bin`.

### 13.2 Ejecución

`qemu-system-i386 -fda os-image.bin -monitor stdio`

### 13.3 Depuración

Modo espera GDB:

`qemu-system-i386 -fda os-image.bin -S -gdb tcp::1234`

---

## 14. Validación funcional

### 14.1 Casos de prueba sugeridos

Arranque:

- Verificar banner y prompt operativo.

Procesos:

- `exec app1`, `exec app2`, `top`.
- Cambio de prioridad y ver efecto en selección.
- `sleep` y reactivación por ticks.

Modo usuario/syscalls:

- `runu <pid>` y comprobación de salida por syscall de escritura.

Servicios:

- `login operator operator123`.
- Arranque/parada/reinicio y consulta de estado.
- `server audit` para revisar trazas.

FS simulado:

- Creación de directorio/archivo, escritura, lectura.
- Cambio de permisos y validación.
- Backup/restore.

### 14.2 Criterios de aceptación

- Sin cuelgues en flujo nominal.
- Comandos responden consistentemente.
- Tick de sistema mantiene avance de scheduler y métricas.
- Control de roles respeta restricciones.

---

## 15. Análisis técnico de fortalezas

- Diseño modular comprensible y escalable.
- Integración funcional de camino kernel-usuario.
- Interrupciones operativas para reloj y teclado.
- Shell rica para entorno académico.
- Simulación de administración de servicios útil para prácticas.

---

## 16. Limitaciones actuales

- Memoria sin paginación ni aislamiento real completo.
- Sin sistema de archivos persistente en bloque.
- Sin multiterminal ni modelo de procesos avanzado.
- Seguridad de autenticación básica (entorno didáctico).
- Dependencia de emulación para validación rápida.

---

## 17. Proyección de evolución

Líneas de continuidad recomendadas:

1. Administración de memoria con paging.
2. IPC y sincronización (colas, mutex, semáforos).
3. Sistema de archivos real (FAT-like).
4. Drivers de almacenamiento y red.
5. Métricas y trazas para profiling de scheduler.

---

## 18. Conclusiones

`MI SO v3.3` constituye una base sólida para formación en sistemas operativos. Integra de forma coherente elementos fundamentales de arquitectura x86, núcleo modular, planificación de procesos, interrupciones y frontera kernel/usuario. Su diseño favorece extensiones progresivas y experimentación académica controlada.

El proyecto cumple con los objetivos de un prototipo didáctico de mediana complejidad y ofrece una plataforma realista para escalar hacia funciones clásicas de un sistema operativo completo.

---

## Anexo A. Estructura de archivos principal

- `boot.asm`
- `linker.ld`
- `MakeFile`
- `src/kernel.c`
- `src/interrupts.c`
- `src/process.c`
- `src/shell.c`
- `src/servers.c`
- `src/vga.c`
- `src/utils.c`
- `include/common.h`
- `include/process.h`
- `include/interrupts.h`
- `include/servers.h`
- `include/shell.h`
- `include/vga.h`
- `include/utils.h`

---

## Anexo B. Glosario breve

- **BIOS:** firmware que inicia el arranque clásico.
- **GDT:** tabla de descriptores de segmentos.
- **IDT:** tabla de descriptores de interrupción.
- **IRQ:** línea de interrupción de hardware.
- **Ring 0 / Ring 3:** niveles de privilegio del procesador.
- **Syscall:** llamada controlada desde usuario al kernel.
- **Freestanding:** entorno C sin biblioteca estándar hospedada.

