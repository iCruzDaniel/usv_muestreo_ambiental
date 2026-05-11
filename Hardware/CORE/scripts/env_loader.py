import os
Import("env")

# ─── ENV LOADER SCRIPT ───────────────────────────────────────────────────────
# Este script carga variables desde el archivo .env y las inyecta como
# macros de compilación (-D) automáticamente.
# ─────────────────────────────────────────────────────────────────────────────

# Ruta al archivo .env (en la raíz del proyecto)
env_file = os.path.join(env.get("PROJECT_DIR"), ".env")

if os.path.exists(env_file):
    print("\n[ENV] Cargando configuración desde .env...")
    env_vars = {}
    with open(env_file) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            
            if "=" in line:
                key, value = line.split("=", 1)
                key = key.strip()
                value = value.strip().strip('"').strip("'")
                env_vars[key] = value

    # Expansión de variables (reemplaza ${VAR} por su valor)
    for key, value in env_vars.items():
        expanded_value = value
        for var_key, var_val in env_vars.items():
            placeholder = "${" + var_key + "}"
            if placeholder in expanded_value:
                expanded_value = expanded_value.replace(placeholder, var_val)
        
        # Mapeo especial
        macro_name = key
        if key == "USV_ID":
            macro_name = "CLIENT_ID"
        
        # Inyectar como definición de C++
        if expanded_value.isdigit():
            env.Append(CPPDEFINES=[(macro_name, expanded_value)])
        else:
            env.Append(CPPDEFINES=[(macro_name, f'\\"{expanded_value}\\"')])
        
        print(f"      - {macro_name} detectada")
    
    print("[ENV] Configuración inyectada con éxito.\n")
else:
    print("\n[ENV] ADVERTENCIA: No se encontró el archivo .env en la raíz.")
    print("[ENV] Se usarán los valores por defecto definidos en include/config.h\n")
