/* modelo_logic3.c - adaptador del ProjectModel a LOGIC3 (plan 16.5).
 *
 * El proyecto se escribe con el escritor canonico. Los montajes antiguos
 * conservan su conversion a piezas `sistema.*`; un LOG 2 con nodos, escenas
 * o scripts se convierte en escenas, objetos, eventos y bloques codigo.lobo
 * reales para que el motor pueda ejecutarlo sin pasar por el lienzo visual.
 */
#include "modelo_logic3.h"

#include "logic3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int id;
    Bloque3 *bloque;
} ReferenciaBloque;

static void ml3_copiar(char *dst, size_t tam, const char *src) {
    size_t n;
    if (!dst || tam == 0) return;
    if (!src) src = "";
    n = strlen(src);
    if (n >= tam) n = tam - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static const PropiedadModelo *ml3_propiedad(const EntidadModelo *e,
                                            const char *clave) {
    int i;
    if (!e || !clave) return NULL;
    for (i = 0; i < e->num_propiedades; i++)
        if (strcmp(e->propiedades[i].clave, clave) == 0)
            return &e->propiedades[i];
    return NULL;
}

static const char *ml3_valor(const EntidadModelo *e, const char *clave) {
    const PropiedadModelo *p = ml3_propiedad(e, clave);
    return p ? p->valor : "";
}

static int ml3_entero(const EntidadModelo *e, const char *clave, int defecto) {
    const char *texto = ml3_valor(e, clave);
    char *fin;
    long n;
    if (!texto[0]) return defecto;
    n = strtol(texto, &fin, 10);
    if (*fin || n < -2147483647L || n > 2147483647L) return defecto;
    return (int)n;
}

static unsigned int ml3_color(const EntidadModelo *e, unsigned int defecto) {
    const char *texto = ml3_valor(e, "color");
    char *fin;
    unsigned long n;
    if (!texto[0]) return defecto;
    n = strtoul(texto, &fin, 16);
    if (*fin) return defecto;
    return (unsigned int)n;
}

static const char *ml3_tipo(const EntidadModelo *e) {
    const char *tipo = ml3_valor(e, "tipo");
    if (tipo[0]) return tipo;
    switch (e->kind) {
    case PM_KIND_ESCENA:    return "sistema.escena";
    case PM_KIND_ACTOR:     return "sistema.actor";
    case PM_KIND_INSTANCIA: return "sistema.clon";
    case PM_KIND_CONTROL:   return "sistema.teclado";
    case PM_KIND_ACCION:    return "sistema.accion.movimiento";
    case PM_KIND_RECURSO:   return "sistema.recurso.imagen";
    default:                return "";
    }
}

static int ml3_campo(Bloque3 *b, const char *clave, const char *valor) {
    int i;
    if (!b || !clave || !valor) return 0;
    for (i = 0; i < b->num_campos; i++) {
        if (strcmp(b->campos[i].clave, clave) != 0) continue;
        ml3_copiar(b->campos[i].valor, sizeof(b->campos[i].valor), valor);
        return 1;
    }
    if (b->num_campos >= L3_CAMPOS_MAX) return 0;
    ml3_copiar(b->campos[b->num_campos].clave,
               sizeof(b->campos[b->num_campos].clave), clave);
    ml3_copiar(b->campos[b->num_campos].valor,
               sizeof(b->campos[b->num_campos].valor), valor);
    b->campos[b->num_campos].linea = b->linea;
    b->num_campos++;
    return 1;
}

static void ml3_cabecera(Proyecto3 *p, const ProjectModel *m) {
    const EntidadModelo *proyecto = NULL;
    int i;
    for (i = 0; i < m->num_entidades; i++) {
        if (m->entidades[i].kind == PM_KIND_PROYECTO) {
            proyecto = &m->entidades[i];
            break;
        }
    }
    if (!proyecto) return;
    ml3_copiar(p->nombre, sizeof(p->nombre), proyecto->nombre);
    p->ancho = ml3_entero(proyecto, "ancho", p->ancho);
    p->alto = ml3_entero(proyecto, "alto", p->alto);
    p->limitar_cuadros = ml3_entero(proyecto, "capas", p->limitar_cuadros);
    p->fps = ml3_entero(proyecto, "fps", p->fps);
    p->velocidad = ml3_entero(proyecto, "modo", p->velocidad);
    p->fondo = ml3_color(proyecto, p->fondo);
}

static int ml3_crear_pieza(Proyecto3 *p, const EntidadModelo *e,
                           ReferenciaBloque *referencias, int *num_referencias) {
    Bloque3 *b = NULL;
    char numero[32];
    char xy[L3_TEXTO_MAX];
    const char *tipo = ml3_tipo(e);
    if (!tipo[0]) return 1;
    if (!logic3_bloque_nuevo(p, 1, 0, p->num_bloques, "sistema.pieza", &b))
        return 0;
    b->linea = e->origen.linea;
    snprintf(numero, sizeof(numero), "%d", e->id);
    snprintf(xy, sizeof(xy), "%d,%d", e->ui.x, e->ui.y);
    if (!ml3_campo(b, "ui", "1") ||
        !ml3_campo(b, "pieza", tipo) ||
        !ml3_campo(b, "id", numero) ||
        !ml3_campo(b, "xy", xy) ||
        !ml3_campo(b, "nombre", e->nombre[0] ? e->nombre : tipo) ||
        !ml3_campo(b, "enlace", ml3_valor(e, "enlace")) ||
        !ml3_campo(b, "valor", ml3_valor(e, "valor")) ||
        !ml3_campo(b, "valor2", ml3_valor(e, "valor2")))
        return 0;
    referencias[*num_referencias].id = e->id;
    referencias[*num_referencias].bloque = b;
    (*num_referencias)++;
    return 1;
}

static int ml3_id_referencia(const EntidadModelo *e, const char *nombre) {
    int i;
    if (!e) return 0;
    for (i = 0; i < e->num_referencias; i++)
        if (strcmp(e->referencias[i].nombre, nombre) == 0)
            return e->referencias[i].id;
    return 0;
}

static Bloque3 *ml3_bloque_id(ReferenciaBloque *referencias, int num, int id) {
    int i;
    for (i = 0; i < num; i++)
        if (referencias[i].id == id) return referencias[i].bloque;
    return NULL;
}

static void ml3_aplicar_conexiones(const ProjectModel *m,
                                   ReferenciaBloque *referencias, int num) {
    int i;
    for (i = 0; i < m->num_entidades; i++) {
        const EntidadModelo *e = &m->entidades[i];
        Bloque3 *origen;
        char numero[32];
        int destino;
        if (e->kind != PM_KIND_CONEXION) continue;
        origen = ml3_bloque_id(referencias, num,
                               ml3_id_referencia(e, "origen"));
        destino = ml3_id_referencia(e, "destino");
        if (!origen || destino <= 0) continue;
        snprintf(numero, sizeof(numero), "%d", destino);
        (void)ml3_campo(origen, "enlace", numero);
    }
}

/* ------------------------------------------------------------------------- */
/* LOG 2 jerarquico -> escenas, objetos y scripts LOGIC3                     */
/* ------------------------------------------------------------------------- */

static const EntidadModelo *ml3_entidad(const ProjectModel *m, int id) {
    int i;
    if (!m || id <= 0) return NULL;
    for (i = 0; i < m->num_entidades; i++)
        if (m->entidades[i].id == id) return &m->entidades[i];
    return NULL;
}

static int ml3_referencia(const EntidadModelo *e, const char *nombre) {
    int i;
    if (!e || !nombre) return 0;
    for (i = 0; i < e->num_referencias; i++)
        if (strcmp(e->referencias[i].nombre, nombre) == 0)
            return e->referencias[i].id;
    return 0;
}

static int ml3_jerarquico(const ProjectModel *m) {
    int i;
    if (!m) return 0;
    for (i = 0; i < m->num_entidades; i++) {
        int kind = m->entidades[i].kind;
        if (kind == PM_KIND_NODO || kind == PM_KIND_SCRIPT ||
            kind == PM_KIND_BINDING)
            return 1;
    }
    return 0;
}

static void ml3_base_archivo(const ProjectModel *m, char *dst, int tam) {
    const char *ruta = NULL;
    const char *p;
    int i;
    if (!dst || tam <= 0) return;
    dst[0] = 0;
    if (m) {
        for (i = 0; i < m->num_entidades; i++) {
            if (m->entidades[i].origen.archivo[0]) {
                ruta = m->entidades[i].origen.archivo;
                break;
            }
        }
    }
    if (!ruta || !ruta[0]) {
        ml3_copiar(dst, (size_t)tam, ".");
        return;
    }
    p = ruta + strlen(ruta);
    while (p > ruta && p[-1] != '/' && p[-1] != '\\') p--;
    if (p == ruta) {
        ml3_copiar(dst, (size_t)tam, ".");
        return;
    }
    {
        size_t n = (size_t)(p - ruta);
        if (n >= (size_t)tam) n = (size_t)tam - 1;
        memcpy(dst, ruta, n);
        dst[n] = 0;
    }
}

static int ml3_ruta_segura(const char *ruta) {
    const char *p;
    if (!ruta || !ruta[0]) return 0;
    if (ruta[0] == '/' || ruta[0] == '\\' ||
        (ruta[0] && ruta[1] == ':')) return 0;
    p = ruta;
    while (*p) {
        const char *inicio = p;
        while (*p && *p != '/' && *p != '\\') p++;
        if ((p - inicio) == 2 && inicio[0] == '.' && inicio[1] == '.')
            return 0;
        while (*p == '/' || *p == '\\') p++;
    }
    return 1;
}

static int ml3_leer_script(const ProjectModel *m, const EntidadModelo *script,
                           char *codigo, int tam, char *diagnostico,
                           size_t longitud) {
    char base[PM_RUTA_MAX * 2];
    char ruta[PM_RUTA_MAX * 3];
    const char *relativa = ml3_valor(script, "ruta");
    FILE *f;
    long largo;
    size_t leidos;

    if (!codigo || tam <= 0) return 0;
    codigo[0] = 0;
    if (!ml3_ruta_segura(relativa)) {
        ml3_copiar(diagnostico, longitud, "ruta de script absoluta o insegura");
        return 0;
    }
    ml3_base_archivo(m, base, sizeof(base));
    snprintf(ruta, sizeof(ruta), "%s/%s", base, relativa);
    f = fopen(ruta, "rb");
    if (!f) {
        snprintf(ruta, sizeof(ruta), "%s", relativa);
        f = fopen(ruta, "rb");
    }
    if (!f) {
        snprintf(ruta, sizeof(ruta), "script %d no encontrado: %s",
                 script ? script->id : 0, relativa);
        ml3_copiar(diagnostico, longitud, ruta);
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    largo = ftell(f);
    if (largo < 0 || largo >= tam) {
        fclose(f);
        snprintf(ruta, sizeof(ruta), "script %d demasiado largo: %s",
                 script ? script->id : 0, relativa);
        ml3_copiar(diagnostico, longitud, ruta);
        return 0;
    }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    leidos = largo > 0 ? fread(codigo, 1, (size_t)largo, f) : 0;
    fclose(f);
    if (leidos != (size_t)largo) {
        ml3_copiar(diagnostico, longitud, "no se pudo leer el script externo");
        codigo[0] = 0;
        return 0;
    }
    codigo[leidos] = 0;
    return 1;
}

static const EntidadModelo *ml3_recurso(const ProjectModel *m, int id) {
    const EntidadModelo *e = ml3_entidad(m, id);
    return e && e->kind == PM_KIND_RECURSO ? e : NULL;
}

static int ml3_escena_de_nodo(const EntidadModelo *e) {
    return ml3_entero(e, "escena", ml3_referencia(e, "escena"));
}

static int ml3_nodo_raiz(const ProjectModel *m, int escena) {
    int i;
    for (i = 0; i < m->num_entidades; i++) {
        const EntidadModelo *e = &m->entidades[i];
        if (e->kind != PM_KIND_NODO || ml3_escena_de_nodo(e) != escena)
            continue;
        if (ml3_entero(e, "padre", ml3_referencia(e, "padre")) <= 0)
            return e->id;
    }
    return 0;
}

static int ml3_objeto_indice(const Proyecto3 *p, int id) {
    int i;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id == id) return i;
    return -1;
}

static void ml3_preparar_traje(const ProjectModel *m, const EntidadModelo *nodo,
                               Objeto3 *objeto) {
    int recurso_id = ml3_referencia(nodo, "recurso");
    const EntidadModelo *recurso = ml3_recurso(m, recurso_id);
    Traje3 *t;
    if (!recurso || objeto->num_trajes >= L3_TRAJES_MAX) return;
    if (strcmp(ml3_valor(recurso, "tipo"), "map") == 0) return;
    t = &objeto->trajes[objeto->num_trajes++];
    memset(t, 0, sizeof(*t));
    t->indice = 0;
    ml3_copiar(t->imagen, sizeof(t->imagen), ml3_valor(recurso, "ruta"));
    t->ancho = ml3_entero(nodo, "ancho", 16);
    t->alto = ml3_entero(nodo, "alto", 16);
    t->cuadros = 1;
    t->velocidad = 1;
    t->linea = recurso->origen.linea;
}

static int ml3_evento_desde_binding(const char *nombre, Evento3 *evento,
                                    int escena) {
    int tipo;
    if (!nombre || !evento) return 0;
    memset(evento, 0, sizeof(*evento));
    evento->objeto = -1;
    if (strcmp(nombre, "escena_entrar") == 0) {
        evento->tipo = L3_EVENTO_ESCENA;
        evento->escena = escena;
        return 1;
    }
    if (strcmp(nombre, "tecla_pulsar") == 0) {
        evento->tipo = L3_EVENTO_TECLA;
        evento->flanco = L3_FLANCO_PULSAR;
        return 1;
    }
    if (strcmp(nombre, "soltar_tecla") == 0) {
        evento->tipo = L3_EVENTO_TECLA;
        evento->flanco = L3_FLANCO_SOLTAR;
        return 1;
    }
    tipo = logic3_evento_desde_texto(nombre);
    if (!tipo || tipo == L3_EVENTO_LLAMADA) return 0;
    evento->tipo = tipo;
    evento->flanco = L3_FLANCO_MANTENER;
    if (tipo == L3_EVENTO_TOCAR) evento->objeto = -1;
    if (tipo == L3_EVENTO_CADA) evento->cada = 1;
    return 1;
}

static int ml3_generar_jerarquico(const ProjectModel *m, const char *ruta,
                                  char *diagnostico, size_t longitud) {
    Proyecto3 p;
    char error[L3_ERROR_MAX];
    char codigo[FUNCIONES_CODIGO_MAX];
    int i, j, orden = 0, siguiente_evento = 1;
    int ancho, alto;

    logic3_iniciar(&p);
    error[0] = 0;
    ml3_cabecera(&p, m);
    ancho = p.ancho;
    alto = p.alto;

    {
        int pasada;
        /* LOGIC3 arranca la primera escena del archivo. Ordenamos la escena
         * marcada como inicial antes de las demas sin cambiar sus IDs. */
        for (pasada = 0; pasada < 2; pasada++) {
            for (i = 0; i < m->num_entidades; i++) {
                const EntidadModelo *e = &m->entidades[i];
                Escena3 *escena;
                int inicial;
                if (e->kind != PM_KIND_ESCENA) continue;
                inicial = ml3_entero(e, "inicial", 0) != 0;
                if ((pasada == 0) != inicial) continue;
                if (p.num_escenas >= L3_ESCENAS_MAX) {
                    ml3_copiar(diagnostico, longitud, "demasiadas escenas");
                    goto error;
                }
                escena = &p.escenas[p.num_escenas++];
                memset(escena, 0, sizeof(*escena));
                escena->id = e->id;
                ml3_copiar(escena->nombre, sizeof(escena->nombre), e->nombre);
                escena->ancho = ml3_entero(e, "ancho", ancho > 0 ? ancho : 640);
                escena->alto = ml3_entero(e, "alto", alto > 0 ? alto : 480);
                escena->fondo = p.fondo;
                escena->linea = e->origen.linea;
                for (j = 0; j < m->num_entidades; j++) {
                    const EntidadModelo *nodo = &m->entidades[j];
                    const EntidadModelo *recurso;
                    int recurso_id;
                    if (nodo->kind != PM_KIND_NODO ||
                        ml3_escena_de_nodo(nodo) != e->id)
                        continue;
                    if (strcmp(ml3_valor(nodo, "tipo"), "map") != 0) continue;
                    recurso_id = ml3_referencia(nodo, "recurso");
                    recurso = ml3_recurso(m, recurso_id);
                    if (recurso) {
                        ml3_copiar(escena->mapa, sizeof(escena->mapa),
                                   ml3_valor(recurso, "ruta"));
                        break;
                    }
                }
            }
        }
    }
    if (p.num_escenas == 0) {
        ml3_copiar(diagnostico, longitud, "el modelo no contiene escenas");
        goto error;
    }
    p.objetos = (Objeto3 *)calloc(L3_OBJETOS_MAX, sizeof(Objeto3));
    if (!p.objetos) {
        ml3_copiar(diagnostico, longitud, "sin memoria para los objetos del modelo");
        goto error;
    }
    p.cap_objetos = L3_OBJETOS_MAX;
    for (i = 0; i < m->num_entidades; i++) {
        const EntidadModelo *e = &m->entidades[i];
        Objeto3 *objeto;
        int escena_id;
        const char *tipo;
        if (e->kind != PM_KIND_NODO) continue;
        escena_id = ml3_escena_de_nodo(e);
        if (!ml3_entidad(m, escena_id) ||
            ml3_entidad(m, escena_id)->kind != PM_KIND_ESCENA ||
            p.num_objetos >= L3_OBJETOS_MAX) {
            ml3_copiar(diagnostico, longitud, "nodo sin escena o demasiados objetos");
            goto error;
        }
        objeto = &p.objetos[p.num_objetos++];
        memset(objeto, 0, sizeof(*objeto));
        objeto->id = e->id;
        ml3_copiar(objeto->nombre, sizeof(objeto->nombre), e->nombre);
        objeto->escena = escena_id;
        objeto->padre = ml3_entero(e, "padre", ml3_referencia(e, "padre"));
        objeto->x = ml3_entero(e, "x", e->ui.x);
        objeto->y = ml3_entero(e, "y", e->ui.y);
        tipo = ml3_valor(e, "tipo");
        objeto->w = ml3_entero(e, "w", strcmp(tipo, "root") == 0 ? ancho : 16);
        objeto->h = ml3_entero(e, "h", strcmp(tipo, "root") == 0 ? alto : 16);
        if (objeto->w <= 0) objeto->w = 16;
        if (objeto->h <= 0) objeto->h = 16;
        objeto->visible = ml3_entero(e, "visible", strcmp(tipo, "root") == 0 ? 0 : 1);
        objeto->capa = ml3_entero(e, "capa", 0);
        objeto->linea = e->origen.linea;
        ml3_preparar_traje(m, e, objeto);
        if (strcmp(tipo, "camera2d") == 0 && !p.camara.objetivo) {
            int zoom = ml3_entero(e, "zoom", 1);
            p.camara.objetivo = objeto->padre > 0 ? objeto->padre : objeto->id;
            p.camara.zoom = (zoom == 2 || zoom == 4) ? zoom : 1;
            p.camara.linea = e->origen.linea;
        }
    }
    for (i = 0; i < m->num_entidades; i++) {
        const EntidadModelo *binding = &m->entidades[i];
        const EntidadModelo *script;
        const EntidadModelo *objetivo;
        Evento3 evento;
        Objeto3 *objeto;
        Bloque3 *bloque;
        int script_id, objetivo_id, objetivo_tipo, indice;
        const char *nombre_evento;
        if (binding->kind != PM_KIND_BINDING) continue;
        script_id = ml3_referencia(binding, "script");
        script = ml3_entidad(m, script_id);
        objetivo_tipo = strcmp(ml3_valor(binding, "objetivo_tipo"), "escena") == 0;
        objetivo_id = ml3_referencia(binding, objetivo_tipo ? "escena" : "nodo");
        if (objetivo_tipo) objetivo_id = ml3_nodo_raiz(m, objetivo_id);
        objetivo = ml3_entidad(m, objetivo_id);
        indice = ml3_objeto_indice(&p, objetivo_id);
        nombre_evento = ml3_valor(binding, "evento");
        if (!script || script->kind != PM_KIND_SCRIPT || !objetivo || indice < 0) {
            ml3_copiar(diagnostico, longitud, "asociacion sin script, nodo u objetivo valido");
            goto error;
        }
        if (!ml3_evento_desde_binding(nombre_evento, &evento,
                                      ml3_escena_de_nodo(objetivo))) {
            snprintf(error, sizeof(error), "evento no soportado: %s", nombre_evento);
            ml3_copiar(diagnostico, longitud, error);
            goto error;
        }
        if (!ml3_leer_script(m, script, codigo, sizeof(codigo), diagnostico,
                             longitud)) goto error;
        objeto = &p.objetos[indice];
        if (objeto->num_eventos >= L3_EVENTOS_OBJETO_MAX) {
            ml3_copiar(diagnostico, longitud, "demasiados eventos en un nodo");
            goto error;
        }
        evento.id = siguiente_evento++;
        evento.linea = binding->origen.linea;
        objeto->eventos[objeto->num_eventos++] = evento;
        if (!logic3_bloque_nuevo(&p, evento.id, 0, orden++, "codigo.lobo",
                                 &bloque)) {
            ml3_copiar(diagnostico, longitud, "no caben los scripts del modelo");
            goto error;
        }
        bloque->linea = script->origen.linea;
        bloque->tiene_codigo = 1;
        ml3_copiar(bloque->codigo, sizeof(bloque->codigo), codigo);
    }
    if (!logic3_validar(&p, error, sizeof(error))) {
        ml3_copiar(diagnostico, longitud, error);
        goto error;
    }
    if (!logic3_guardar(&p, ruta, error, sizeof(error))) {
        ml3_copiar(diagnostico, longitud, error);
        goto error;
    }
    logic3_liberar(&p);
    return 1;

error:
    logic3_liberar(&p);
    return 0;
}

int ml3_generar(const ProjectModel *m, const char *ruta,
                char *diagnostico, size_t longitud) {
    Proyecto3 proyecto;
    ReferenciaBloque referencias[PM_ENTIDADES_MAX];
    char error[L3_ERROR_MAX];
    const char *mensaje = "";
    int num_referencias = 0;
    int ok = 0;
    int i;

    if (diagnostico && longitud > 0) diagnostico[0] = '\0';
    if (!m || !ruta || !ruta[0]) {
        ml3_copiar(diagnostico, longitud, "falta el modelo o la ruta de salida");
        return 0;
    }

    if (ml3_jerarquico(m))
        return ml3_generar_jerarquico(m, ruta, diagnostico, longitud);

    logic3_iniciar(&proyecto);
    error[0] = '\0';
    ml3_cabecera(&proyecto, m);
    for (i = 0; i < m->num_entidades; i++) {
        const EntidadModelo *e = &m->entidades[i];
        if (e->kind == PM_KIND_PROYECTO || e->kind == PM_KIND_CONEXION)
            continue;
        if (num_referencias >= PM_ENTIDADES_MAX) {
            mensaje = "demasiadas piezas para el LOGIC3";
            goto fin;
        }
        if (!ml3_crear_pieza(&proyecto, e, referencias, &num_referencias)) {
            mensaje = "no cabe la pieza en el LOGIC3";
            goto fin;
        }
    }
    ml3_aplicar_conexiones(m, referencias, num_referencias);
    if (!logic3_validar(&proyecto, error, sizeof(error))) {
        mensaje = error;
        goto fin;
    }
    if (!logic3_guardar(&proyecto, ruta, error, sizeof(error))) {
        mensaje = error;
        goto fin;
    }
    ok = 1;

fin:
    if (!ok) ml3_copiar(diagnostico, longitud, mensaje);
    logic3_liberar(&proyecto);
    return ok;
}
