# Configuración de Clang-Format en STM32CubeIDE

Para asegurar que todo el equipo de **Araba Motorsport** escribe código con el mismo estilo, utilizamos la herramienta estándar de la industria `clang-format`. El repositorio contiene un archivo `.clang-format` en la raíz con las reglas definidas.

Sigue estos pasos para configurarlo en tu STM32CubeIDE (basado en Eclipse):

## 1. Instalar el Plugin "CppStyle" (Si no viene por defecto)
Las versiones más nuevas de CubeIDE pueden traer soporte nativo, pero la forma más segura es usar CppStyle.
1. Abre STM32CubeIDE.
2. En el menú superior, ve a **Help -> Eclipse Marketplace...**
3. En el buscador escribe **"CppStyle"**.
4. Haz clic en **Install**, acepta la licencia y reinicia el IDE cuando te lo pida.

## 2. Configurar el formateador
1. Una vez reiniciado, ve a **Window -> Preferences** (o *Eclipse -> Preferences* en macOS).
2. Despliega el menú **C/C++** en la barra lateral izquierda y entra en **Formatting**.
3. En el menú desplegable llamado *Active profile* o *Formatter*, asegúrate de seleccionar **CppStyle** (por defecto suele estar en *Built-in*).
4. Dale a **Apply and Close**.

*Opcional para proyectos concretos:*
Si quieres que esta regla se aplique **sólo** a este repositorio:
1. Haz clic derecho sobre la carpeta raíz del proyecto `AMS_GitHub` en tu explorador de proyectos (Project Explorer).
2. Entra en **Properties**.
3. Ve a **C/C++ General -> Formatter**.
4. Marca la casilla **"Enable project specific settings"**.
5. Selecciona **CppStyle** en el menú desplegable.
6. Dale a **Apply and Close**.

## 3. ¿Cómo se usa?
*   A partir de ahora, cuando estés editando cualquier archivo `.c` o `.h`, simplemente pulsa **`Ctrl + Shift + F`** (o haz clic derecho en el código -> *Source -> Format*).
*   El IDE leerá automáticamente el archivo `.clang-format` que está en la raíz del proyecto y aplicará las sangrías (4 espacios), alineación de punteros y llaves exactamente igual para todos los miembros del equipo.
*   **Nota importante:** El código generado por STM32CubeMX entre los tags `/* USER CODE BEGIN ... */` se formateará perfectamente sin romper lo que el generador interno hace.
