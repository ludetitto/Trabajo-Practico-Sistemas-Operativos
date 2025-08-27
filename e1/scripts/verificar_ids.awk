# verificar_ids.awk
# Verifica que los IDs (columna 1) de un archivo CSV sean:
# - únicos (sin duplicados)
# - correlativos (sin saltos)

BEGIN {
    FS = ","  # separador de campos CSV
}

NR > 1 {
    id = $1 + 0  # Convierte a número por las dudas
    ids[id]++    # Cuenta cuántas veces aparece cada ID
    if (id > max) max = id
    if (min == 0 || id < min) min = id
}

END {
    duplicados = 0
    for (id in ids) {
        if (ids[id] > 1) {
            print "❌ ID duplicado encontrado:", id
            duplicados++
        }
    }

    cantidad_ids_unicos = length(ids)
    total_esperado = max - min + 1

    print "📊 ID mínimo:", min
    print "📊 ID máximo:", max
    print "📊 Cantidad de IDs únicos:", cantidad_ids_unicos
    print "📊 Esperados (correlativos):", total_esperado

    if (duplicados == 0 && cantidad_ids_unicos == total_esperado) {
        print "✅ Todos los IDs son únicos y correlativos."
        exit 0
    } else {
        print "❌ Error: hay IDs duplicados o saltos en la numeración."
        exit 1
    }
}
