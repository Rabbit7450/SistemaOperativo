Uso de GitHub Actions para compilar el SO y obtener `os-image.bin`

Pasos:

1. Hacer push a la rama `main` (o ejecutar manualmente el workflow desde la pestaña "Actions").
2. GitHub Actions instalará dependencias en un runner Ubuntu, ejecutará `make` dentro de `mi_so_completo` y generará `os-image.bin`.
3. El workflow sube `os-image.bin` como artifact descargable desde la ejecución del workflow.

Cómo ejecutar manualmente:

1. Ve a la sección "Actions" del repositorio en GitHub.
2. Selecciona "Build OS Image" y pulsa "Run workflow".
3. Espera a que termine y descarga el artifact `os-image`.

Notas:
- No modifica nada en tu PC.
- Si quieres que incluya también la ejecución en QEMU dentro del workflow para capturar salida en logs, puedo añadirlo (recomendado solo para pruebas no interactivas).
