# GUIA COMPLETA DE DIAPOSITIVAS - MI SO v3.3

## Slide 1 - Portada
**Titulo:** MI SO v3.3  
**Subtitulo:** Sistema Operativo educativo modular en arquitectura x86 (32-bit)  
**Autor:** Adalit  
**Asignatura:** Sistemas Operativos  
**Fecha:** [colocar fecha de defensa]  

**Nota del expositor (decir):**  
"Este proyecto implementa un sistema operativo educativo desde el arranque hasta la ejecucion de comandos, con enfoque en arquitectura modular y aprendizaje practico."

**Visual sugerido:**  
Diagrama simple: BIOS -> Bootloader -> Kernel -> Shell

---

## Slide 2 - Introduccion
**Titulo:** Introduccion

**Contenido (copiar tal cual):**
- En la mayoria de cursos, los sistemas operativos se estudian de forma teorica.
- Este proyecto propone una aproximacion practica: construir un SO funcional desde cero.
- Se desarrolla un entorno donde se entienden en conjunto:
  - arranque del sistema,
  - interrupciones,
  - procesos,
  - llamadas al sistema,
  - consola de comandos.
- El objetivo es aprender haciendo, con una base extensible para futuras mejoras.

**Nota del expositor:**  
"La motivacion principal es pasar de conceptos abstractos a una implementacion real y verificable."

---

## Slide 3 - Concepto del sistema
**Titulo:** Concepto del sistema

**Contenido (copiar tal cual):**
- MI SO v3.3 es un sistema operativo educativo de bajo nivel.
- Usa:
  - Bootloader en Assembly (arranque),
  - Kernel en C freestanding,
  - Ejecucion en QEMU (i386).
- El sistema implementa:
  - modo protegido 32-bit,
  - manejo de interrupciones (IDT + PIC),
  - planificacion de procesos,
  - shell interactiva,
  - comandos de gestion de servicios simulados.

**Mensaje clave de la slide:**  
Proyecto didactico funcional, no solo teorico.

---

## Slide 4 - Arquitectura general
**Titulo:** Arquitectura general (vision modular)

**Contenido (copiar tal cual):**
- Bootloader:
  - carga kernel y activa modo protegido.
- Kernel:
  - inicializa subsistemas y controla ejecucion global.
- Interrupciones:
  - timer, teclado, excepciones y syscall.
- Procesos:
  - estados, prioridad, cambio de contexto.
- Shell:
  - interpreta comandos del usuario.
- Servicios simulados:
  - administracion por roles y FS logico.

**Visual sugerido:**  
Bloques por modulo con flechas de dependencia.

---

## Slide 5 - Flujo funcional del sistema
**Titulo:** Flujo de funcionamiento

**Contenido (copiar tal cual):**
1. BIOS carga el bootloader.
2. Bootloader carga el kernel en memoria.
3. Kernel inicializa procesos, servicios e IDT.
4. Se habilitan interrupciones.
5. La shell queda disponible con prompt `>`.
6. El usuario ejecuta comandos y recibe respuesta inmediata.

**Nota del expositor:**  
"Este flujo explica todo el ciclo de vida, desde encendido hasta operacion interactiva."

---

## Slide 6 - Comandos principales (sistema)
**Titulo:** Comandos principales - Sistema

**Contenido (copiar tal cual):**
- `help` -> muestra comandos disponibles.
- `about` -> informacion de version del sistema.
- `clear` -> limpia pantalla.
- `uptime` -> tiempo de actividad.
- `memory` -> memoria usada por procesos.
- `whoami` -> rol y estado de sesion.

**Frase de cierre de slide:**  
Estos comandos dan visibilidad operativa basica del sistema.

---

## Slide 7 - Comandos principales (procesos)
**Titulo:** Comandos principales - Procesos

**Contenido (copiar tal cual):**
- `ps` -> lista de procesos.
- `top` -> monitor detallado (estado, prioridad, memoria, CPU).
- `exec <nombre>` -> crea proceso.
- `runu <pid>` -> ejecuta proceso en modo usuario.
- `sleep <pid> <seg>` -> suspende temporalmente.
- `priority <pid> <0-9>` -> cambia prioridad.
- `kill <pid>` -> termina proceso.

**Frase de cierre de slide:**  
Permiten crear, observar y controlar el ciclo de vida de procesos.

---

## Slide 8 - Comandos principales (servicios y FS)
**Titulo:** Comandos principales - Servicios y archivos

**Contenido (copiar tal cual):**
- Gestion de servicios:
  - `server list`
  - `server status <srv>`
  - `server start|stop|restart <srv>`
  - `server backup|restore <srv>`
  - `server audit`
- Sistema de archivos simulado:
  - `server fs dirs|mkdir|touch|ls|write|cat|rm|rmdir|chmod`

**Frase de cierre de slide:**  
El sistema integra administracion operativa con control de acceso por rol.

---

## Slide 9 - Caso practico funcional (contexto)
**Titulo:** Caso practico funcional

**Escenario (copiar tal cual):**
Se detecta una incidencia en un servicio critico.  
El operador debe validar estado, aplicar accion correctiva y registrar evidencia.

**Objetivos del caso:**
1. Iniciar sesion.
2. Revisar estado de servicios.
3. Reiniciar servicio afectado.
4. Confirmar resultado.
5. Verificar auditoria.

---

## Slide 10 - Caso practico funcional (paso a paso)
**Titulo:** Caso practico - Secuencia de comandos

**Contenido (copiar tal cual):**
1. `login operator operator123`
2. `server list`
3. `server status nginx`
4. `server restart nginx`
5. `server status nginx`
6. `server audit`

**Resultado esperado (copiar tal cual):**
- Sesion activa como operador.
- Servicio detectado y operado correctamente.
- Registro de accion visible en auditoria.

**Visual sugerido:**  
Mock de terminal con salida antes/despues.

---

## Slide 11 - Caso practico funcional (procesos)
**Titulo:** Caso practico - Gestion de procesos

**Contenido (copiar tal cual):**
1. `exec app1`
2. `exec app2`
3. `top`
4. `priority 1 2`
5. `sleep 2 5`
6. `top`

**Resultado esperado (copiar tal cual):**
- Creacion correcta de procesos.
- Cambios observables en prioridad y estado.
- Replanificacion visible en monitor.

---

## Slide 12 - Buenas practicas
**Titulo:** Buenas practicas de uso y operacion

**Contenido (copiar tal cual):**
- Validar entorno con `help` y `principales` antes de operar.
- Confirmar sesion y rol antes de acciones sensibles.
- Usar `server backup` antes de cambios de archivos.
- Verificar impacto con `top` y `server status`.
- Revisar `server audit` al finalizar operaciones.
- Mantener comandos en secuencias cortas y verificables.

**Mensaje clave:**  
Operar con trazabilidad y validacion continua.

---

## Slide 13 - Recomendaciones
**Titulo:** Recomendaciones tecnicas y academicas

**Contenido (copiar tal cual):**
- Continuar evolucion del sistema en tres frentes:
  1. memoria (paginacion),
  2. syscalls adicionales,
  3. persistencia real en disco.
- Mantener arquitectura modular para facilitar mantenimiento.
- Incorporar pruebas por escenarios reproducibles.
- Fortalecer autenticacion y auditoria de eventos.
- Documentar cada version con cambios y resultados de validacion.

---

## Slide 14 - Conclusiones
**Titulo:** Conclusiones

**Contenido (copiar tal cual):**
- Se construyo un sistema operativo educativo funcional de extremo a extremo.
- El proyecto integra arranque, kernel, interrupciones, procesos, syscall y shell.
- Se demuestra gestion operativa mediante comandos y casos practicos.
- La base actual es suficiente para evolucionar hacia capacidades avanzadas.
- El resultado valida el enfoque de aprendizaje practico en sistemas operativos.

**Frase final de defensa (copiar tal cual):**  
"MI SO v3.3 no solo explica como funciona un sistema operativo: lo demuestra en ejecucion."

---

## Slide 15 - Cierre y preguntas
**Titulo:** Gracias - Preguntas

**Contenido sugerido:**
- "Gracias por su atencion"
- "Preguntas y comentarios"

**Tip:**  
Dejar en pequeno un recordatorio del flujo:
Boot -> Kernel -> Interrupts -> Process -> Shell

---

## Material visual minimo a preparar
- 1 diagrama de arquitectura.
- 1 diagrama de flujo de arranque.
- 4 capturas/mock de terminal:
  - comandos de sistema,
  - gestion de procesos,
  - caso de servicio,
  - auditoria.
- 1 slide de respaldo con comandos principales completos.

---

## Plan de exposicion por tiempo (20 minutos)
- Slide 1-3: 4 min
- Slide 4-5: 3 min
- Slide 6-8: 4 min
- Slide 9-11: 5 min
- Slide 12-14: 3 min
- Slide 15: 1 min

---

## Checklist final antes de presentar
- Todas las slides tienen titulo claro y mensaje unico.
- Se cubren exactamente los puntos solicitados:
  - Introduccion
  - Concepto
  - Comandos principales
  - Caso practico funcional
  - Buenas practicas
  - Recomendacion
  - Conclusiones
- El caso practico muestra secuencia completa y resultado esperado.
- Se puede entender la presentacion sin ver el sistema en vivo.

