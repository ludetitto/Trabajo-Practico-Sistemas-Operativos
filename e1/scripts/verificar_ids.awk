# verificar_ids.awk
# Verifica que los IDs (columna 1) de un archivo CSV cumplan:
# - Ser correlativos (1..N sin saltos)
# - No tener duplicados

BEGIN {
    FS = ","   # separador CSV
    min = 0
    max = 0
}

NR == 1 {
    # Validar que el header tenga "id" como primer campo
    if ($1 != "id") {
        print "❌ ERROR: el primer campo del encabezado no es 'id', encontrado:", $1
        exit 1
    }
    next
}

NR > 1 {
    id = $1 + 0
    if (id <= 0) {
        print "❌ ERROR: ID inválido en línea", NR, ":", $1
        exit 1
    }

    if (seen[id]++) {
        print "❌ ERROR: ID duplicado encontrado:", id
        exit 1
    }

    if (min == 0 || id < min) min = id
    if (id > max) max = id
}

END {
    total_ids = length(seen)
    esperados = max - min + 1

    print "📊 ID mínimo:", min
    print "📊 ID máximo:", max
    print "📊 Cantidad de IDs únicos:", total_ids
    print "📊 Esperados (correlativos):", esperados

    if (min != 1) {
        print "❌ ERROR: el ID mínimo debería ser 1"
        exit 1
    }

    for (i = 1; i <= max; i++) {
        if (!(i in seen)) {
            print "❌ ERROR: falta el ID", i
            exit 1
        }
    }

    if (total_ids != esperados) {
        print "❌ ERROR: hay huecos en la numeración"
        exit 1
    }

    print "✅ Todos los IDs son únicos y correlativos (1..", max, ")"
    exit 0
}
