/* migracion.c - importacion del gestor visual antiguo a Inicio.
 *
 * La capa no modifica los lectores de LOGIC1/LOGIC2/LOGIC3 ni el escritor
 * canonico. Solo lee el archivo original con esos lectores, construye un
 * ProjectModel jerarquico y lo guarda como LOG 2 para que Inicio lo abra.
 * Todo lo que no tiene equivalente directo se escribe en el informe.
 */
#include "migracion.h"

#include "app.h"
#include "inicio.h"
#include "logic3.h"
#include "modelo_log.h"
#include "sistema.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIG_INFORME_MAX    16384
#define MIG_FUNCIONES_MAX  64
#define MIG_ENLACES_MAX    256
#define MIG_BUF_CODIGO     FUNCIONES_CODIGO_MAX
#define MIG_BUF_RUTA       (PM_RUTA_MAX * 3)

typedef struct {
    char viejo;
    int antiguo;
    int nuevo;
} MigracionPar;

typedef struct {
    int origen;
    int destino;
    int linea;
} MigracionEnlace;

typedef struct {
    char informe[MIG_INFORME_MAX];
    int avisos;
    char base[MIG_BUF_RUTA];
    char funciones[MIG_FUNCIONES_MAX][L3_NOMBRE_MAX];
    int parametros[MIG_FUNCIONES_MAX];
    int num_funciones;
    int num_scripts;
} Migracion;

/* ------------------------------------------------------------------ */
/* Utilidades de texto y archivos                                      */
/* ------------------------------------------------------------------ */

static void mig_copiar(char *dst, size_t tam, const char *src) {
    size_t n;
    if (!dst || tam == 0) return;
    if (!src) src = "";
    n = strlen(src);
    if (n >= tam) n = tam - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void mig_anadir_texto(char *dst, size_t tam, const char *texto) {
    size_t usado;
    if (!dst || tam == 0 || !texto) return;
    usado = strlen(dst);
    if (usado >= tam - 1) return;
    mig_copiar(dst + usado, tam - usado, texto);
}

static void mig_anadir_formato(char *dst, size_t tam, const char *formato, ...) {
    char tmp[1024];
    va_list args;
    va_start(args, formato);
    vsnprintf(tmp, sizeof(tmp), formato, args);
    va_end(args);
    mig_anadir_texto(dst, tam, tmp);
}

static void mig_anotar(Migracion *c, const char *formato, ...) {
    char linea[1024];
    va_list args;
    va_start(args, formato);
    vsnprintf(linea, sizeof(linea), formato, args);
    va_end(args);
    mig_anadir_formato(c->informe, sizeof(c->informe), "%s\n", linea);
}

static void mig_avisar(Migracion *c, const char *formato, ...) {
    char linea[1024];
    va_list args;
    va_start(args, formato);
    vsnprintf(linea, sizeof(linea), formato, args);
    va_end(args);
    c->avisos++;
    mig_anadir_formato(c->informe, sizeof(c->informe), "aviso: %s\n", linea);
}

static int mig_numero_valido(const char *texto) {
    char *fin;
    if (!texto || !*texto) return 0;
    strtol(texto, &fin, 10);
    return *fin == '\0';
}

static void mig_identificador(const char *src, char *dst, size_t tam) {
    size_t i, j = 0;
    if (!dst || tam == 0) return;
    if (!src || !*src) {
        mig_copiar(dst, tam, "f");
        return;
    }
    for (i = 0; src[i] && j + 1 < tam; i++) {
        unsigned char ch = (unsigned char)src[i];
        int vale = ch == '_' || isalpha(ch) || (j > 0 && isdigit(ch));
        if (vale) dst[j++] = (char)ch;
    }
    if (!j) dst[j++] = 'f';
    dst[j] = '\0';
}

/* Los numeros se conservan como literales; el resto se reduce a un nombre
 * valido de lobo para que un campo_extra no rompa el script generado. */
static void mig_valor_lobo(const char *src, char *dst, size_t tam) {
    if (mig_numero_valido(src)) mig_copiar(dst, tam, src);
    else mig_identificador(src, dst, tam);
}

static void mig_argumentos(const char *src, char *dst, size_t tam, int *numero) {
    const char *p = src ? src : "";
    int n = 0;
    dst[0] = '\0';
    if (numero) *numero = 0;
    while (*p && n < 8) {
        char pieza[PM_TEXTO_MAX];
        char limpio[PM_TEXTO_MAX];
        size_t k = 0;
        while (*p == ',' || *p == ' ' || *p == '\t' || *p == ';') p++;
        if (!*p) break;
        while (*p && *p != ',' && *p != ';' && *p != '\t' &&
               k + 1 < sizeof(pieza))
            pieza[k++] = *p++;
        pieza[k] = '\0';
        mig_valor_lobo(pieza, limpio, sizeof(limpio));
        if (n) mig_anadir_texto(dst, tam, ", ");
        mig_anadir_texto(dst, tam, limpio);
        n++;
    }
    if (numero) *numero = n;
}

static void mig_base_ruta(const char *ruta, char *dst, size_t tam) {
    const char *fin;
    size_t n;
    if (!ruta || !*ruta) {
        mig_copiar(dst, tam, ".");
        return;
    }
    fin = ruta + strlen(ruta);
    while (fin > ruta && fin[-1] != '/' && fin[-1] != '\\') fin--;
    if (fin == ruta) {
        mig_copiar(dst, tam, ".");
        return;
    }
    n = (size_t)(fin - ruta);
    if (n >= tam) n = tam - 1;
    memcpy(dst, ruta, n);
    dst[n] = '\0';
}

static int mig_copiar_archivo(const char *origen, const char *destino) {
    unsigned char buf[8192];
    FILE *f, *d;
    size_t leidos;

    if (!origen || !*origen || !destino || !*destino) return 0;
    if (strcmp(origen, destino) == 0) return 0;
    f = fopen(origen, "rb");
    if (!f) return 0;
    d = fopen(destino, "wb");
    if (!d) {
        fclose(f);
        return 0;
    }
    while ((leidos = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (fwrite(buf, 1, leidos, d) != leidos) {
            fclose(f);
            fclose(d);
            return 0;
        }
    }
    {
        int cierre_f = fclose(f);
        int cierre_d = fclose(d);
        return cierre_f == 0 && cierre_d == 0;
    }
}

static int mig_escribir_texto(const char *ruta, const char *texto) {
    FILE *f;
    if (!ruta || !*ruta || !texto) return 0;
    f = fopen(ruta, "w");
    if (!f) return 0;
    fputs(texto, f);
    fputc('\n', f);
    fclose(f);
    return 1;
}

static void mig_ruta_script(Migracion *c, const char *prefijo, int id,
                            char *relativa, size_t tam) {
    snprintf(relativa, tam, "Scripts/%s_%d.lobo", prefijo, id);
    if (!c->num_scripts) {
        char carpeta[MIG_BUF_RUTA];
        snprintf(carpeta, sizeof(carpeta), "%s/Scripts", c->base);
        crear_carpeta(carpeta);
    }
    c->num_scripts++;
}

static int mig_escribir_script(Migracion *c, const char *relativa,
                               const char *codigo) {
    char absoluta[MIG_BUF_RUTA];
    snprintf(absoluta, sizeof(absoluta), "%s/%s", c->base, relativa);
    return mig_escribir_texto(absoluta, codigo);
}

/* ------------------------------------------------------------------ */
/* Mapas de identificadores                                            */
/* ------------------------------------------------------------------ */

static void mig_mapear(MigracionPar *mapa, int *numero, int antiguo, int nuevo) {
    int i;
    if (!mapa || !numero || antiguo <= 0 || nuevo <= 0) return;
    for (i = 0; i < *numero; i++)
        if (mapa[i].antiguo == antiguo) {
            mapa[i].nuevo = nuevo;
            return;
        }
    if (*numero >= PM_ENTIDADES_MAX) return;
    mapa[*numero].viejo = 1;
    mapa[*numero].antiguo = antiguo;
    mapa[*numero].nuevo = nuevo;
    (*numero)++;
}

static int mig_nuevo_id(const MigracionPar *mapa, int numero, int antiguo) {
    int i;
    if (antiguo <= 0) return 0;
    for (i = 0; i < numero; i++)
        if (mapa[i].antiguo == antiguo) return mapa[i].nuevo;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Funciones y codigo lobo generado                                    */
/* ------------------------------------------------------------------ */

static int mig_tiene_funcion(const Migracion *c, const char *nombre) {
    int i;
    if (!c || !nombre || !nombre[0]) return 0;
    for (i = 0; i < c->num_funciones; i++)
        if (strcmp(c->funciones[i], nombre) == 0) return 1;
    return 0;
}

static void mig_anadir_funcion(Migracion *c, const char *nombre, int parametros) {
    char limpio[L3_NOMBRE_MAX];
    mig_valor_lobo(nombre, limpio, sizeof(limpio));
    if (mig_tiene_funcion(c, limpio)) return;
    if (c->num_funciones >= MIG_FUNCIONES_MAX) {
        mig_avisar(c, "funcion %s: no cabe en el script migrado", limpio);
        return;
    }
    mig_copiar(c->funciones[c->num_funciones], L3_NOMBRE_MAX, limpio);
    if (parametros < 0) parametros = 0;
    if (parametros > 8) {
        mig_avisar(c, "funcion %s: mas de 8 parametros, se reducen", limpio);
        parametros = 8;
    }
    c->parametros[c->num_funciones] = parametros;
    c->num_funciones++;
    mig_avisar(c, "funcion %s: cuerpo original no migrado", limpio);
}

static void mig_escribir_funciones(const Migracion *c, char *codigo, size_t tam) {
    int i, p;
    codigo[0] = '\0';
    for (i = 0; i < c->num_funciones; i++) {
        mig_anadir_formato(codigo, tam, "funcion %s(", c->funciones[i]);
        for (p = 0; p < c->parametros[i]; p++)
            mig_anadir_formato(codigo, tam, "a%d%s", p + 1,
                               p + 1 < c->parametros[i] ? "," : "");
        mig_anadir_texto(codigo, tam, ")\n  retornar 0\nfin\n\n");
    }
}

typedef struct {
    const char *texto;
    const char *tecla;
    const char *eje;
    int signo;
} MigracionDireccion;

static const MigracionDireccion *mig_direccion(const char *texto) {
    static const MigracionDireccion tabla[] = {
        { "izquierda", "tecla_izquierda", "x", -1 },
        { "derecha",   "tecla_derecha",   "x",  1 },
        { "arriba",    "tecla_arriba",    "y", -1 },
        { "abajo",     "tecla_abajo",     "y",  1 },
        { "salto",     "tecla_salto",     "y", -1 }
    };
    size_t i;
    if (!texto || !*texto) return NULL;
    for (i = 0; i < sizeof(tabla) / sizeof(tabla[0]); i++)
        if (strstr(texto, tabla[i].texto) != NULL) return &tabla[i];
    return NULL;
}

static const char *mig_evento_numero(int evento, Migracion *c) {
    switch (evento) {
    case 1:  return "inicio";
    case 2:  return "fotograma";
    case 3:  return "tecla";
    case 4:  return "recibir";
    case 5:  return "tocar";
    case 6:  return "cada";
    case 7:  return "clonar";
    case 8:  return "escena";
    case 9:
        mig_avisar(c, "evento 9 (llamada) no tiene equivalente; se usa fotograma");
        return "fotograma";
    case 10: return "escena_salir";
    default: return NULL;
    }
}

static const char *mig_evento_flanco(const char *texto) {
    if (!texto || !*texto) return NULL;
    if (strcmp(texto, "pulsar") == 0 || strcmp(texto, "1") == 0)
        return "tecla_pulsar";
    if (strcmp(texto, "soltar") == 0 || strcmp(texto, "2") == 0)
        return "soltar_tecla";
    if (strcmp(texto, "mantener") == 0 || strcmp(texto, "0") == 0 ||
        strcmp(texto, "teclado") == 0 || strcmp(texto, "raton") == 0 ||
        strcmp(texto, "mando") == 0)
        return "tecla";
    return NULL;
}

static int mig_evento_conocido(const char *evento) {
    static const char *const validos[] = {
        "fotograma", "inicio", "tecla", "recibir", "tocar", "cada",
        "clonar", "escena", "escena_entrar", "tecla_pulsar",
        "soltar_tecla", "escena_salir"
    };
    size_t i;
    if (!evento || !*evento) return 0;
    for (i = 0; i < sizeof(validos) / sizeof(validos[0]); i++)
        if (strcmp(validos[i], evento) == 0) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Acceso al ProjectModel de entrada                                   */
/* ------------------------------------------------------------------ */

static const EntidadModelo *mig_entidad(const ProjectModel *m, int id) {
    int i;
    if (!m || id <= 0) return NULL;
    for (i = 0; i < m->num_entidades; i++)
        if (m->entidades[i].id == id) return &m->entidades[i];
    return NULL;
}

static const char *mig_prop(const EntidadModelo *e, const char *clave) {
    int i;
    if (!e || !clave) return "";
    for (i = 0; i < e->num_propiedades; i++)
        if (strcmp(e->propiedades[i].clave, clave) == 0)
            return e->propiedades[i].valor;
    return "";
}

static const char *mig_prop_o(const EntidadModelo *e, const char *a,
                              const char *b) {
    const char *valor = mig_prop(e, a);
    return valor[0] ? valor : mig_prop(e, b);
}

static int mig_prop_entero(const EntidadModelo *e, const char *clave,
                           int defecto) {
    const char *texto = mig_prop(e, clave);
    if (!texto[0]) return defecto;
    return (int)strtol(texto, NULL, 10);
}

static int mig_ref(const EntidadModelo *e, const char *nombre) {
    int i;
    if (!e || !nombre) return 0;
    for (i = 0; i < e->num_referencias; i++)
        if (strcmp(e->referencias[i].nombre, nombre) == 0)
            return e->referencias[i].id;
    return 0;
}

static int mig_origen_conexion(const ProjectModel *m, int destino) {
    int i;
    if (!m || destino <= 0) return 0;
    for (i = 0; i < m->num_entidades; i++) {
        const EntidadModelo *e = &m->entidades[i];
        if (e->kind == PM_KIND_CONEXION && mig_ref(e, "destino") == destino)
            return mig_ref(e, "origen");
    }
    return 0;
}

static int mig_kind_pieza(const char *tipo) {
    if (!tipo || !*tipo) return 0;
    if (strcmp(tipo, "sistema.escena") == 0) return PM_KIND_ESCENA;
    if (strcmp(tipo, "sistema.actor") == 0) return PM_KIND_ACTOR;
    if (strcmp(tipo, "sistema.teclado") == 0 ||
        strcmp(tipo, "sistema.control") == 0) return PM_KIND_CONTROL;
    if (strcmp(tipo, "sistema.recurso.imagen") == 0 ||
        strcmp(tipo, "sistema.recurso.mapa") == 0) return PM_KIND_RECURSO;
    if (strncmp(tipo, "sistema.accion", 14) == 0) return PM_KIND_ACCION;
    return PM_KIND_NODO;
}

static int mig_es_nodo(const ProjectModel *m, int id) {
    const EntidadModelo *e = mig_entidad(m, id);
    return e && e->kind == PM_KIND_NODO;
}

/* ------------------------------------------------------------------ */
/* Paso 1: escenas, recursos, nodos y jerarquia                        */
/* ------------------------------------------------------------------ */

static int mig_crear_escenas(const ProjectModel *origen, ProjectModel *dest,
                             Migracion *c, MigracionPar *mapa, int *num_mapa,
                             MigracionPar *raices, int *num_raices) {
    const EntidadModelo *proyecto = NULL;
    EntidadModelo *e;
    char numero[32];
    int i, id_proyecto, ancho = 640, alto = 480, primero = 0;

    for (i = 0; i < origen->num_entidades; i++)
        if (origen->entidades[i].kind == PM_KIND_PROYECTO) {
            proyecto = &origen->entidades[i];
            break;
        }
    if (!proyecto) return 0;
    ancho = mig_prop_entero(proyecto, "ancho", 640);
    alto = mig_prop_entero(proyecto, "alto", 480);

    id_proyecto = pm_anadir(dest, PM_KIND_PROYECTO, proyecto->nombre);
    e = pm_entidad(dest, id_proyecto);
    if (!e) return 0;
    pm_origen(e, proyecto->origen.archivo, proyecto->origen.linea);
    mig_copiar(numero, sizeof(numero), mig_prop(proyecto, "ancho"));
    pm_propiedad(e, "ancho", numero[0] ? numero : "640", 0);
    mig_copiar(numero, sizeof(numero), mig_prop(proyecto, "alto"));
    pm_propiedad(e, "alto", numero[0] ? numero : "480", 0);
    mig_copiar(numero, sizeof(numero), mig_prop(proyecto, "capas"));
    pm_propiedad(e, "capas", numero[0] ? numero : "1", 0);
    mig_copiar(numero, sizeof(numero), mig_prop(proyecto, "fps"));
    pm_propiedad(e, "fps", numero[0] ? numero : "60", 0);
    mig_copiar(numero, sizeof(numero), mig_prop(proyecto, "modo"));
    pm_propiedad(e, "modo", numero[0] ? numero : "3", 0);
    pm_propiedad(e, "color", mig_prop(proyecto, "color"), 0);
    pm_propiedad(e, "migrado", "1", 0);
    mig_mapear(mapa, num_mapa, proyecto->id, id_proyecto);

    for (i = 0; i < origen->num_entidades; i++) {
        const EntidadModelo *vieja = &origen->entidades[i];
        EntidadModelo *escena, *raiz;
        int id_escena, id_raiz;
        if (vieja->kind != PM_KIND_ESCENA) continue;
        id_escena = pm_anadir(dest, PM_KIND_ESCENA, vieja->nombre);
        escena = pm_entidad(dest, id_escena);
        if (!escena) return 0;
        pm_origen(escena, vieja->origen.archivo, vieja->origen.linea);
        pm_propiedad(escena, "inicial", primero++ ? "0" : "1",
                     vieja->origen.linea);
        pm_propiedad(escena, "ancho",
                     mig_prop(vieja, "ancho")[0] ? mig_prop(vieja, "ancho") :
                     (ancho > 0 ? "640" : "640"), vieja->origen.linea);
        {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d", alto > 0 ? alto : 480);
            pm_propiedad(escena, "alto",
                         mig_prop(vieja, "alto")[0] ? mig_prop(vieja, "alto") : tmp,
                         vieja->origen.linea);
        }
        pm_propiedad(escena, "color", mig_prop(vieja, "color"),
                     vieja->origen.linea);

        id_raiz = pm_anadir(dest, PM_KIND_NODO, "Raiz");
        raiz = pm_entidad(dest, id_raiz);
        if (!raiz) return 0;
        pm_origen(raiz, vieja->origen.archivo, vieja->origen.linea);
        snprintf(numero, sizeof(numero), "%d", id_escena);
        pm_propiedad(raiz, "escena", numero, vieja->origen.linea);
        pm_propiedad(raiz, "tipo", "root", vieja->origen.linea);
        pm_propiedad(raiz, "visible", "0", vieja->origen.linea);
        pm_propiedad(raiz, "w", "640", vieja->origen.linea);
        pm_propiedad(raiz, "h", "480", vieja->origen.linea);
        pm_referencia(raiz, "escena", id_escena, NULL, vieja->origen.linea);
        mig_mapear(mapa, num_mapa, vieja->id, id_escena);
        mig_mapear(raices, num_raices, vieja->id, id_raiz);
        mig_anotar(c, "convertida escena=%d nombre=%s raiz=%d", id_escena,
                   escena->nombre, id_raiz);
    }

    if (!*num_raices) {
        EntidadModelo *escena, *raiz;
        int id_escena = pm_anadir(dest, PM_KIND_ESCENA, "Escena migrada");
        int id_raiz = pm_anadir(dest, PM_KIND_NODO, "Raiz");
        escena = pm_entidad(dest, id_escena);
        raiz = pm_entidad(dest, id_raiz);
        if (!escena || !raiz) return 0;
        pm_propiedad(escena, "inicial", "1", 0);
        pm_propiedad(escena, "ancho", "640", 0);
        pm_propiedad(escena, "alto", "480", 0);
        snprintf(numero, sizeof(numero), "%d", id_escena);
        pm_propiedad(raiz, "escena", numero, 0);
        pm_propiedad(raiz, "tipo", "root", 0);
        pm_propiedad(raiz, "visible", "0", 0);
        pm_referencia(raiz, "escena", id_escena, NULL, 0);
        mig_mapear(raices, num_raices, 0, id_raiz);
        mig_anotar(c, "convertida escena=%d nombre=Escena_migrada raiz=%d",
                   id_escena, id_raiz);
    }
    return 1;
}

static int mig_crear_recursos(const ProjectModel *origen, ProjectModel *dest,
                              Migracion *c, MigracionPar *mapa, int *num_mapa) {
    int i;
    for (i = 0; i < origen->num_entidades; i++) {
        const EntidadModelo *vieja = &origen->entidades[i];
        EntidadModelo *nueva;
        char numero[32];
        int id;
        if (vieja->kind != PM_KIND_RECURSO) continue;
        id = pm_anadir(dest, PM_KIND_RECURSO, vieja->nombre);
        nueva = pm_entidad(dest, id);
        if (!nueva) return 0;
        pm_origen(nueva, vieja->origen.archivo, vieja->origen.linea);
        pm_propiedad(nueva, "tipo", mig_prop(vieja, "tipo"), vieja->origen.linea);
        pm_propiedad(nueva, "ruta", mig_prop(vieja, "ruta"), vieja->origen.linea);
        snprintf(numero, sizeof(numero), "%d", id);
        pm_propiedad(nueva, "id", numero, vieja->origen.linea);
        mig_mapear(mapa, num_mapa, vieja->id, id);
        mig_anotar(c, "convertido recurso=%d nombre=%s ruta=%s", id,
                   nueva->nombre, mig_prop(nueva, "ruta"));
    }
    return 1;
}

static int mig_crear_nodos(const ProjectModel *origen, ProjectModel *dest,
                           Migracion *c, MigracionPar *mapa, int *num_mapa,
                           const MigracionPar *raices, int num_raices) {
    int i;
    int raiz = mig_nuevo_id(raices, num_raices, 0);
    if (!raiz && num_raices > 0) raiz = raices[0].nuevo;

    for (i = 0; i < origen->num_entidades; i++) {
        const EntidadModelo *vieja = &origen->entidades[i];
        EntidadModelo *nueva;
        char numero[32];
        int id;
        if (vieja->kind != PM_KIND_ACTOR && vieja->kind != PM_KIND_CONTROL &&
            vieja->kind != PM_KIND_NODO) continue;
        id = pm_anadir(dest, PM_KIND_NODO, vieja->nombre);
        nueva = pm_entidad(dest, id);
        if (!nueva) return 0;
        pm_origen(nueva, vieja->origen.archivo, vieja->origen.linea);
        snprintf(numero, sizeof(numero), "%d", raiz);
        pm_propiedad(nueva, "escena", numero, vieja->origen.linea);
        {
            const char *tipo = mig_prop(vieja, "tipo");
            if (!tipo[0])
                tipo = vieja->kind == PM_KIND_CONTROL ? "sistema.control" :
                       vieja->kind == PM_KIND_ACTOR ? "sistema.actor" : "nodo";
            pm_propiedad(nueva, "tipo", tipo, vieja->origen.linea);
        }
        pm_propiedad(nueva, "x", "0", vieja->origen.linea);
        pm_propiedad(nueva, "y", "0", vieja->origen.linea);
        pm_propiedad(nueva, "entrada", mig_prop_o(vieja, "valor", "entrada"),
                     vieja->origen.linea);
        pm_propiedad(nueva, "dispositivo",
                     mig_prop_o(vieja, "valor2", "dispositivo"),
                     vieja->origen.linea);
        pm_referencia(nueva, "escena", raiz, NULL, vieja->origen.linea);
        pm_ui(nueva, vieja->ui.x, vieja->ui.y, 16, 16, 0, 0, 0);
        mig_mapear(mapa, num_mapa, vieja->id, id);
        mig_anotar(c, "convertido nodo=%d nombre=%s tipo=%s", id,
                   nueva->nombre, mig_prop(nueva, "tipo"));
    }
    return 1;
}

/* La escena del nodo debe ser la entidad PM_KIND_ESCENA, no la raiz. */
static int mig_fijar_escena_nodos(ProjectModel *dest, const MigracionPar *raices,
                                  int num_raices) {
    int i;
    for (i = 0; i < dest->num_entidades; i++) {
        EntidadModelo *e = &dest->entidades[i];
        int raiz, escena;
        if (e->kind != PM_KIND_NODO) continue;
        if (strcmp(mig_prop(e, "tipo"), "root") == 0) continue;
        raiz = mig_ref(e, "escena");
        escena = mig_ref(mig_entidad(dest, raiz), "escena");
        if (escena <= 0) {
            if (num_raices > 0) escena = mig_ref(mig_entidad(dest, raices[0].nuevo),
                                                 "escena");
        }
        if (escena <= 0) return 0;
        {
            char numero[32];
            snprintf(numero, sizeof(numero), "%d", escena);
            pm_propiedad(e, "escena", numero, e->origen.linea);
        }
        pm_referencia(e, "escena", escena, NULL, e->origen.linea);
    }
    return 1;
}

static void mig_fijar_padres(const ProjectModel *origen, ProjectModel *dest,
                             MigracionPar *mapa, int num_mapa,
                             const MigracionPar *raices, int num_raices) {
    int i;
    int primera_raiz = num_raices > 0 ? raices[0].nuevo : 0;
    for (i = 0; i < origen->num_entidades; i++) {
        const EntidadModelo *vieja = &origen->entidades[i];
        EntidadModelo *nueva;
        int id = mig_nuevo_id(mapa, num_mapa, vieja->id);
        int fuente, padre = 0;
        if (!mig_es_nodo(dest, id)) continue;
        nueva = pm_entidad(dest, id);
        fuente = mig_origen_conexion(origen, vieja->id);
        if (fuente > 0) {
            int fuente_nueva = mig_nuevo_id(mapa, num_mapa, fuente);
            if (mig_es_nodo(dest, fuente_nueva)) padre = fuente_nueva;
            else {
                int r = mig_nuevo_id(raices, num_raices, fuente);
                if (r > 0) padre = r;
            }
        }
        if (!padre) padre = primera_raiz;
        if (padre > 0 && padre != id)
            pm_referencia(nueva, "padre", padre, NULL, vieja->origen.linea);
    }
}

static void mig_fijar_recursos(const ProjectModel *origen, ProjectModel *dest,
                               MigracionPar *mapa, int num_mapa) {
    int i;
    for (i = 0; i < origen->num_entidades; i++) {
        const EntidadModelo *con = &origen->entidades[i];
        EntidadModelo *nodo;
        int recurso, destino;
        if (con->kind != PM_KIND_CONEXION) continue;
        recurso = mig_nuevo_id(mapa, num_mapa, mig_ref(con, "origen"));
        destino = mig_nuevo_id(mapa, num_mapa, mig_ref(con, "destino"));
        nodo = pm_entidad(dest, destino);
        if (!nodo || nodo->kind != PM_KIND_NODO) continue;
        if (mig_entidad(dest, recurso) &&
            mig_entidad(dest, recurso)->kind == PM_KIND_RECURSO)
            pm_referencia(nodo, "recurso", recurso, NULL, con->origen.linea);
    }
}

/* ------------------------------------------------------------------ */
/* Paso 2: scripts, funciones y asociaciones                           */
/* ------------------------------------------------------------------ */

static const char *mig_evento_accion(const ProjectModel *origen,
                                     const EntidadModelo *accion,
                                     Migracion *c) {
    const char *texto = mig_prop(accion, "evento");
    int numero;
    if (mig_evento_conocido(texto)) return texto;
    if (texto[0] && mig_numero_valido(texto)) {
        numero = (int)strtol(texto, NULL, 10);
        texto = mig_evento_numero(numero, c);
        if (texto) return texto;
    }
    texto = mig_prop_o(accion, "flanco", "valor2");
    if (mig_evento_flanco(texto)) return mig_evento_flanco(texto);
    {
        int control = mig_prop_entero(accion, "control", 0);
        const EntidadModelo *padre = mig_entidad(origen, control);
        if (padre) {
            texto = mig_prop_o(padre, "flanco", "dispositivo");
            if (mig_evento_flanco(texto)) return mig_evento_flanco(texto);
        }
    }
    mig_avisar(c, "accion %d: evento no reconocido, se usa fotograma",
               accion->id);
    return "fotograma";
}

static void mig_generar_codigo(Migracion *c, const EntidadModelo *accion,
                               char *codigo, size_t tam) {
    char tipo[PM_NOMBRE_MAX];
    char valor[PM_TEXTO_MAX];
    char entrada[PM_TEXTO_MAX];
    char funcion[L3_NOMBRE_MAX];
    char funcion_limpia[L3_NOMBRE_MAX];
    char argumentos[PM_TEXTO_MAX];
    char numero[32];
    const MigracionDireccion *dir;
    int nargs = 0, velocidad;

    mig_copiar(tipo, sizeof(tipo), mig_prop(accion, "tipo"));
    if (!tipo[0]) mig_copiar(tipo, sizeof(tipo), "sistema.accion");
    mig_copiar(valor, sizeof(valor), mig_prop_o(accion, "valor", "entrada"));
    mig_copiar(entrada, sizeof(entrada), mig_prop(accion, "entrada"));
    mig_copiar(funcion, sizeof(funcion), mig_prop(accion, "funcion"));
    mig_argumentos(mig_prop(accion, "argumentos"), argumentos,
                   sizeof(argumentos), &nargs);
    if (funcion[0]) {
        mig_valor_lobo(funcion, funcion_limpia, sizeof(funcion_limpia));
        if (!mig_tiene_funcion(c, funcion_limpia))
            mig_anadir_funcion(c, funcion_limpia, nargs);
    }
    mig_escribir_funciones(c, codigo, tam);
    mig_anadir_formato(codigo, tam, "# Migrado desde %s\n", tipo);

    velocidad = mig_prop_entero(accion, "velocidad", 4);
    if (velocidad == 0) velocidad = 4;
    snprintf(numero, sizeof(numero), "%d", velocidad);
    dir = mig_direccion(valor);
    if (!dir) dir = mig_direccion(entrada);
    if (dir) {
        mig_anadir_formato(codigo, tam, "si %s\n  %s = %s %s %d\nfin\n",
                           dir->tecla, dir->eje, dir->eje,
                           dir->signo > 0 ? "+" : "-", velocidad);
    } else if (funcion[0]) {
        mig_valor_lobo(funcion, funcion_limpia, sizeof(funcion_limpia));
        dir = mig_direccion(entrada);
        if (dir)
            mig_anadir_formato(codigo, tam,
                               "si %s\n  %s(%s)\nfin\n",
                               dir->tecla, funcion_limpia, argumentos);
        else
            mig_anadir_formato(codigo, tam, "%s(%s)\n", funcion_limpia,
                               argumentos);
    } else {
        mig_avisar(c, "accion %d sin equivalente exacto; se deja un aviso",
                   accion->id);
        mig_anadir_texto(codigo, tam,
                         "# conversion no exacta: accion sin equivalente\n");
        mig_anadir_texto(codigo, tam, "mensaje \"sin equivalente\"\n");
    }
    (void)numero;
}

static int mig_crear_script(const Migracion *c, ProjectModel *dest,
                            const EntidadModelo *accion, const char *codigo,
                            int destino, const char *evento,
                            const char *diagnostico) {
    char relativa[PM_RUTA_MAX];
    char nombre[PM_NOMBRE_MAX];
    EntidadModelo *e;
    int id_script, id_asoc;

    if (diagnostico && diagnostico[0]) return 0;
    snprintf(nombre, sizeof(nombre), "%s", accion->nombre[0] ? accion->nombre :
             "Migrado");
    mig_ruta_script((Migracion *)c, "accion", accion->id, relativa,
                    sizeof(relativa));
    id_script = pm_anadir(dest, PM_KIND_SCRIPT, nombre);
    e = pm_entidad(dest, id_script);
    if (!e) return 0;
    pm_origen(e, accion->origen.archivo, accion->origen.linea);
    pm_propiedad(e, "ruta", relativa, accion->origen.linea);
    if (!mig_escribir_script((Migracion *)c, relativa, codigo)) return 0;

    id_asoc = pm_anadir(dest, PM_KIND_BINDING, "asociacion");
    e = pm_entidad(dest, id_asoc);
    if (!e) return 0;
    pm_origen(e, accion->origen.archivo, accion->origen.linea);
    pm_propiedad(e, "evento", evento, accion->origen.linea);
    pm_propiedad(e, "objetivo_tipo", "nodo", accion->origen.linea);
    pm_referencia(e, "script", id_script, NULL, accion->origen.linea);
    pm_referencia(e, "nodo", destino, NULL, accion->origen.linea);
    return 1;
}

static int mig_crear_scripts(const ProjectModel *origen, ProjectModel *dest,
                             Migracion *c, MigracionPar *mapa, int num_mapa,
                             const MigracionPar *raices, int num_raices) {
    int i;
    int raiz = num_raices > 0 ? raices[0].nuevo : 0;

    for (i = 0; i < origen->num_entidades; i++) {
        const EntidadModelo *vieja = &origen->entidades[i];
        char codigo[MIG_BUF_CODIGO];
        char diagnostico[PM_ERROR_MAX];
        const char *evento;
        int destino, fuente;

        if (vieja->kind != PM_KIND_ACCION) continue;
        fuente = mig_origen_conexion(origen, vieja->id);
        destino = mig_nuevo_id(mapa, num_mapa, fuente);
        if (!mig_es_nodo(dest, destino)) {
            int actor = mig_prop_entero(vieja, "actor", 0);
            int control = mig_prop_entero(vieja, "control", 0);
            destino = mig_nuevo_id(mapa, num_mapa, control);
            if (!mig_es_nodo(dest, destino))
                destino = mig_nuevo_id(mapa, num_mapa, actor);
        }
        if (!mig_es_nodo(dest, destino)) destino = raiz;
        if (!mig_es_nodo(dest, destino)) {
            mig_avisar(c, "accion %d sin nodo destino", vieja->id);
            continue;
        }
        evento = mig_evento_accion(origen, vieja, c);
        mig_generar_codigo(c, vieja, codigo, sizeof(codigo));
        diagnostico[0] = '\0';
        if (!mig_crear_script(c, dest, vieja, codigo, destino, evento,
                              diagnostico)) {
            mig_avisar(c, "accion %d no se pudo convertir en script", vieja->id);
            continue;
        }
        mig_anotar(c, "convertida accion=%d nombre=%s script=%d nodo=%d evento=%s",
                   vieja->id, vieja->nombre,
                   dest->num_entidades - 1, destino, evento);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Publicacion del resultado                                           */
/* ------------------------------------------------------------------ */

static void mig_contar(const ProjectModel *m, int *escenas, int *nodos,
                       int *recursos, int *scripts, int *asociaciones) {
    int i;
    *escenas = *nodos = *recursos = *scripts = *asociaciones = 0;
    for (i = 0; i < m->num_entidades; i++) {
        switch (m->entidades[i].kind) {
        case PM_KIND_ESCENA:   (*escenas)++; break;
        case PM_KIND_NODO:     (*nodos)++; break;
        case PM_KIND_RECURSO:  (*recursos)++; break;
        case PM_KIND_SCRIPT:   (*scripts)++; break;
        case PM_KIND_BINDING:  (*asociaciones)++; break;
        default: break;
        }
    }
}

/* Toma posesion de `modelo` tanto si acierta como si falla. */
static int mig_publicar(Migracion *c, ProjectModel *modelo, const char *salida,
                        char *diagnostico, size_t longitud) {
    char validacion[PM_ERROR_MAX];
    char lectura[PM_ERROR_MAX];
    ProjectModel *reabierto;
    Inicio *in = inicio_servicio();

    if (pm_validar(modelo, validacion, sizeof(validacion))) {
        mig_copiar(diagnostico, longitud, validacion);
        inicio_modelo_cargar(modelo, salida);
        return 0;
    }
    inicio_modelo_cargar(modelo, salida);
    if (!inicio_modelo_guardar(salida, diagnostico, longitud)) return 0;
    reabierto = ml_leer(salida, lectura, sizeof(lectura));
    if (!reabierto) {
        mig_copiar(validacion, sizeof(validacion), "el resultado no se puede abrir");
        if (lectura[0]) mig_anadir_texto(validacion, sizeof(validacion), ": ");
        mig_anadir_texto(validacion, sizeof(validacion), lectura);
        mig_copiar(diagnostico, longitud, validacion);
        return 0;
    }
    inicio_modelo_cargar(reabierto, salida);
    mig_anotar(c, "resultado abierto en inicio entidades=%d",
               in->modelo.num_entidades);
    mig_copiar(diagnostico, longitud, "sin errores");
    return 1;
}

static int mig_convertir_modelo(const ProjectModel *origen, Migracion *c,
                                const char *salida, char *diagnostico,
                                size_t longitud, int ya_jerarquico) {
    ProjectModel *dest = pm_crear();
    MigracionPar mapa[PM_ENTIDADES_MAX];
    MigracionPar raices[PM_ENTIDADES_MAX];
    int num_mapa = 0, num_raices = 0;
    int escenas, nodos, recursos, scripts, asociaciones;

    if (!dest) {
        mig_copiar(diagnostico, longitud, "sin memoria para el modelo migrado");
        return 0;
    }
    if (ya_jerarquico) {
        pm_copiar(origen, dest);
        mig_anotar(c, "origen=LOG2_jerarquico conversion=ya_jerarquico");
        mig_contar(dest, &escenas, &nodos, &recursos, &scripts, &asociaciones);
        mig_anotar(c, "resumen escenas=%d nodos=%d scripts=%d asociaciones=%d avisos=%d",
                   escenas, nodos, scripts, asociaciones, c->avisos);
        return mig_publicar(c, dest, salida, diagnostico, longitud);
    }

    if (!mig_crear_escenas(origen, dest, c, mapa, &num_mapa,
                            raices, &num_raices) ||
        !mig_crear_recursos(origen, dest, c, mapa, &num_mapa) ||
        !mig_crear_nodos(origen, dest, c, mapa, &num_mapa,
                         raices, num_raices) ||
        !mig_fijar_escena_nodos(dest, raices, num_raices)) {
        mig_copiar(diagnostico, longitud, "no se pudo construir la jerarquia");
        pm_destruir(dest);
        return 0;
    }
    mig_fijar_padres(origen, dest, mapa, num_mapa, raices, num_raices);
    mig_fijar_recursos(origen, dest, mapa, num_mapa);
    if (!mig_crear_scripts(origen, dest, c, mapa, num_mapa,
                            raices, num_raices)) {
        mig_copiar(diagnostico, longitud, "no se pudieron crear los scripts");
        pm_destruir(dest);
        return 0;
    }
    mig_contar(dest, &escenas, &nodos, &recursos, &scripts, &asociaciones);
    mig_anotar(c, "resumen escenas=%d nodos=%d recursos=%d scripts=%d asociaciones=%d avisos=%d",
               escenas, nodos, recursos, scripts, asociaciones, c->avisos);
    return mig_publicar(c, dest, salida, diagnostico, longitud);
}

/* ------------------------------------------------------------------ */
/* Importacion de LOGIC3 con bloques sistema.*                         */
/* ------------------------------------------------------------------ */

static const char *mig_campo(const Bloque3 *b, const char *clave) {
    int i;
    if (!b || !clave) return "";
    for (i = 0; i < b->num_campos; i++)
        if (strcmp(b->campos[i].clave, clave) == 0)
            return b->campos[i].valor;
    return "";
}

static int mig_campo_entero(const Bloque3 *b, const char *clave, int defecto) {
    const char *texto = mig_campo(b, clave);
    if (!texto[0]) return defecto;
    return (int)strtol(texto, NULL, 10);
}

static int mig_anadir_id(ProjectModel *m, int id, int kind, const char *nombre,
                         MigracionPar *mapa, int *num_mapa) {
    int nuevo;
    if (id > 0 && !pm_entidad(m, id))
        nuevo = pm_anadir_con_id(m, id, kind, nombre);
    else
        nuevo = pm_anadir(m, kind, nombre);
    if (!nuevo) return 0;
    if (id > 0) mig_mapear(mapa, num_mapa, id, nuevo);
    return nuevo;
}

static void mig_recurso_desde_traje(ProjectModel *m, Objeto3 *o, int nodo,
                                    int linea, MigracionPar *mapa,
                                    int *num_mapa) {
    char nombre[PM_NOMBRE_MAX];
    char ruta[PM_RUTA_MAX];
    int i, id;
    for (i = 0; i < o->num_trajes; i++) {
        snprintf(nombre, sizeof(nombre), "Traje_%d", i + 1);
        snprintf(ruta, sizeof(ruta), "trajes/%s", o->trajes[i].imagen);
        id = mig_anadir_id(m, 0, PM_KIND_RECURSO, nombre, mapa, num_mapa);
        if (!id) return;
        pm_propiedad(pm_entidad(m, id), "ruta", ruta, linea);
        pm_propiedad(pm_entidad(m, id), "tipo", "image", linea);
        pm_conectar(m, id, "salida", nodo, "entrada", linea);
    }
}

/* Una pieza sistema.escena sustituye a la escena generada. Sin esta lectura
 * previa el montaje antiguo produce dos escenas: la automatica y la pieza. */
static int mig_hay_pieza_escena(const Proyecto3 *p) {
    int i;
    if (!p) return 0;
    for (i = 0; i < p->num_bloques; i++) {
        const Bloque3 *b = &p->bloques[i];
        if (strcmp(b->tipo, "sistema.pieza") != 0) continue;
        if (strcmp(mig_campo(b, "pieza"), "sistema.escena") == 0) return 1;
    }
    return 0;
}

/* Los enlaces se resuelven al final: una pieza puede apuntar a otra que todava
 * no existe cuando se lee su bloque. */
static void mig_cola_enlace(MigracionEnlace *lista, int *numero,
                            int origen, int destino, int linea,
                            Migracion *c) {
    if (origen <= 0 || destino <= 0) return;
    if (*numero >= MIG_ENLACES_MAX) {
        mig_avisar(c, "enlace %d -> %d fuera del limite; no se migra",
                   origen, destino);
        return;
    }
    lista[*numero].origen = origen;
    lista[*numero].destino = destino;
    lista[*numero].linea = linea;
    (*numero)++;
}

static ProjectModel *mig_modelo_desde_logic3(const Proyecto3 *p, Migracion *c,
                                              char *diagnostico,
                                              size_t longitud) {
    ProjectModel *m = pm_crear();
    MigracionPar mapa[PM_ENTIDADES_MAX * 2];
    MigracionEnlace enlaces[MIG_ENLACES_MAX];
    int num_mapa = 0;
    int num_enlaces = 0;
    SistemaPrograma sistema;
    char error[L3_ERROR_MAX];
    char numero[32];
    int i, j, id, escena_primera = 0;

    if (!m) {
        mig_copiar(diagnostico, longitud, "sin memoria para importar LOGIC3");
        return NULL;
    }
    error[0] = '\0';
    id = pm_anadir(m, PM_KIND_PROYECTO, p->nombre);
    if (!id) goto error;
    {
        EntidadModelo *e = pm_entidad(m, id);
        snprintf(numero, sizeof(numero), "%d", p->ancho);
        pm_propiedad(e, "ancho", numero, 0);
        snprintf(numero, sizeof(numero), "%d", p->alto);
        pm_propiedad(e, "alto", numero, 0);
        pm_propiedad(e, "capas", "1", 0);
        snprintf(numero, sizeof(numero), "%d", p->fps);
        pm_propiedad(e, "fps", numero, 0);
        snprintf(numero, sizeof(numero), "%d", p->velocidad);
        pm_propiedad(e, "modo", numero, 0);
        snprintf(numero, sizeof(numero), "%06X", p->fondo);
        pm_propiedad(e, "color", numero, 0);
    }

    for (i = 0; i < p->num_escenas; i++) {
        id = mig_anadir_id(m, p->escenas[i].id, PM_KIND_ESCENA,
                           p->escenas[i].nombre, mapa, &num_mapa);
        if (!id) goto error;
        pm_propiedad(pm_entidad(m, id), "inicial",
                     escena_primera++ ? "0" : "1", p->escenas[i].linea);
        snprintf(numero, sizeof(numero), "%d", p->escenas[i].ancho);
        pm_propiedad(pm_entidad(m, id), "ancho", numero, p->escenas[i].linea);
        snprintf(numero, sizeof(numero), "%d", p->escenas[i].alto);
        pm_propiedad(pm_entidad(m, id), "alto", numero, p->escenas[i].linea);
        snprintf(numero, sizeof(numero), "%06X", p->escenas[i].fondo);
        pm_propiedad(pm_entidad(m, id), "color", numero, p->escenas[i].linea);
    }
    if (!p->num_escenas && !mig_hay_pieza_escena(p)) {
        id = mig_anadir_id(m, 1, PM_KIND_ESCENA, "Escena migrada",
                           mapa, &num_mapa);
        if (!id) goto error;
        pm_propiedad(pm_entidad(m, id), "inicial", "1", 0);
        pm_propiedad(pm_entidad(m, id), "ancho", "640", 0);
        pm_propiedad(pm_entidad(m, id), "alto", "480", 0);
    }

    for (i = 0; i < p->num_objetos && p->objetos; i++) {
        Objeto3 *o = &p->objetos[i];
        int escena = mig_nuevo_id(mapa, num_mapa, o->escena);
        id = mig_anadir_id(m, o->id, PM_KIND_NODO, o->nombre,
                           mapa, &num_mapa);
        if (!id) goto error;
        snprintf(numero, sizeof(numero), "%d", escena > 0 ? escena : 1);
        pm_propiedad(pm_entidad(m, id), "escena", numero, o->linea);
        pm_propiedad(pm_entidad(m, id), "tipo", "actor", o->linea);
        pm_referencia(pm_entidad(m, id), "escena", escena > 0 ? escena : 1,
                      NULL, o->linea);
        if (o->padre > 0) {
            int padre = mig_nuevo_id(mapa, num_mapa, o->padre);
            if (padre > 0)
                pm_conectar(m, padre, "salida", id, "entrada", o->linea);
        }
        mig_recurso_desde_traje(m, o, id, o->linea, mapa, &num_mapa);
    }

    for (i = 0; i < p->num_funciones; i++)
        mig_anadir_funcion(c, p->funciones[i].nombre,
                           p->funciones[i].parametros);

    for (i = 0; i < p->num_bloques; i++) {
        const Bloque3 *b = &p->bloques[i];
        const char *tipo = b->tipo;
        char nombre[PM_NOMBRE_MAX];
        if (strcmp(tipo, "sistema.pieza") == 0) {
            int pieza_id = mig_campo_entero(b, "id", 0);
            int enlace = mig_campo_entero(b, "enlace", 0);
            const char *valor = mig_campo(b, "valor");
            const char *valor2 = mig_campo(b, "valor2");
            const char *nombre_pieza = mig_campo(b, "nombre");
            char xy[64];
            int x = 0, y = 0, kind;
            snprintf(nombre, sizeof(nombre), "%s",
                     nombre_pieza[0] ? nombre_pieza : "Pieza");
            kind = mig_kind_pieza(mig_campo(b, "pieza"));
            id = mig_anadir_id(m, pieza_id, kind, nombre, mapa, &num_mapa);
            if (!id) goto error;
            pm_origen(pm_entidad(m, id), NULL, b->linea);
            pm_propiedad(pm_entidad(m, id), "tipo", mig_campo(b, "pieza"),
                         b->linea);
            mig_copiar(xy, sizeof(xy), mig_campo(b, "xy"));
            if (sscanf(xy, "%d,%d", &x, &y) == 2)
                pm_ui(pm_entidad(m, id), x, y, 16, 16, 0, 0, 0);
            pm_propiedad(pm_entidad(m, id), "valor", valor, b->linea);
            pm_propiedad(pm_entidad(m, id), "valor2", valor2, b->linea);
            if (kind == PM_KIND_RECURSO) {
                char ruta[PM_RUTA_MAX];
                if (valor2[0]) snprintf(ruta, sizeof(ruta), "%s/%s", valor2, valor);
                else mig_copiar(ruta, sizeof(ruta), valor);
                pm_propiedad(pm_entidad(m, id), "ruta", ruta, b->linea);
            }
            mig_cola_enlace(enlaces, &num_enlaces, pieza_id, enlace,
                            b->linea, c);
            continue;
        }
        if (strcmp(tipo, "sistema.control") == 0) {
            int jugador = mig_campo_entero(b, "jugador", 1);
            int actor = mig_campo_entero(b, "actor", 0);
            snprintf(nombre, sizeof(nombre), "Control_%d", jugador);
            id = mig_anadir_id(m, b->id, PM_KIND_CONTROL, nombre,
                               mapa, &num_mapa);
            if (!id) goto error;
            pm_origen(pm_entidad(m, id), NULL, b->linea);
            pm_propiedad(pm_entidad(m, id), "jugador",
                         mig_campo(b, "jugador"), b->linea);
            pm_propiedad(pm_entidad(m, id), "dispositivo",
                         mig_campo(b, "dispositivo"), b->linea);
            pm_propiedad(pm_entidad(m, id), "esquema",
                         mig_campo(b, "esquema"), b->linea);
            pm_propiedad(pm_entidad(m, id), "activo",
                         mig_campo(b, "activo"), b->linea);
            pm_propiedad(pm_entidad(m, id), "actor", mig_campo(b, "actor"),
                         b->linea);
            mig_cola_enlace(enlaces, &num_enlaces, actor, b->id,
                            b->linea, c);
            continue;
        }
        if (strcmp(tipo, "sistema.accion") == 0) {
            snprintf(nombre, sizeof(nombre), "Accion_%d", b->id);
            id = mig_anadir_id(m, b->id, PM_KIND_ACCION, nombre,
                               mapa, &num_mapa);
            if (!id) goto error;
            pm_origen(pm_entidad(m, id), NULL, b->linea);
            pm_propiedad(pm_entidad(m, id), "entrada", mig_campo(b, "entrada"),
                         b->linea);
            pm_propiedad(pm_entidad(m, id), "flanco", mig_campo(b, "flanco"),
                         b->linea);
            pm_propiedad(pm_entidad(m, id), "funcion", mig_campo(b, "funcion"),
                         b->linea);
            pm_propiedad(pm_entidad(m, id), "argumentos",
                         mig_campo(b, "argumentos"), b->linea);
            snprintf(numero, sizeof(numero), "%d", b->padre);
            pm_propiedad(pm_entidad(m, id), "control", numero, b->linea);
            mig_cola_enlace(enlaces, &num_enlaces, b->padre, b->id,
                            b->linea, c);
            continue;
        }
        if (strcmp(tipo, "sistema.regla") == 0) {
            int actor = mig_campo_entero(b, "actor", 0);
            snprintf(nombre, sizeof(nombre), "Regla_%d", b->id);
            id = mig_anadir_id(m, b->id, PM_KIND_ACCION, nombre,
                               mapa, &num_mapa);
            if (!id) goto error;
            pm_origen(pm_entidad(m, id), NULL, b->linea);
            pm_propiedad(pm_entidad(m, id), "evento", mig_campo(b, "evento"),
                         b->linea);
            pm_propiedad(pm_entidad(m, id), "flanco", mig_campo(b, "flanco"),
                         b->linea);
            pm_propiedad(pm_entidad(m, id), "funcion", mig_campo(b, "funcion"),
                         b->linea);
            snprintf(numero, sizeof(numero), "%d", actor);
            pm_propiedad(pm_entidad(m, id), "actor", numero, b->linea);
            mig_cola_enlace(enlaces, &num_enlaces, actor, b->id,
                            b->linea, c);
            continue;
        }
        if (sistema_es_bloque(b))
            mig_anotar(c, "importada pieza=%s sin equivalente directo", tipo);
    }

    for (i = 0; i < num_enlaces; i++) {
        int origen = mig_nuevo_id(mapa, num_mapa, enlaces[i].origen);
        int destino = mig_nuevo_id(mapa, num_mapa, enlaces[i].destino);
        if (origen > 0 && destino > 0 && origen != destino)
            pm_conectar(m, origen, "salida", destino, "entrada",
                        enlaces[i].linea);
    }

    error[0] = '\0';
    if (!sistema_leer(p, &sistema, error, sizeof(error)))
        mig_avisar(c, "modelo de programa incompleto; se importan las piezas sueltas");
    for (j = 0; j < sistema.num_acciones; j++) {
        if (sistema.acciones[j].funcion[0] &&
            !mig_tiene_funcion(c, sistema.acciones[j].funcion))
            mig_anadir_funcion(c, sistema.acciones[j].funcion, 1);
    }
    mig_copiar(diagnostico, longitud, "sin errores");
    return m;

error:
    mig_copiar(diagnostico, longitud,
               error[0] ? error : "no cabe el proyecto en el modelo");
    pm_destruir(m);
    return NULL;
}

static int mig_es_jerarquico(const ProjectModel *m) {
    int i;
    for (i = 0; i < m->num_entidades; i++) {
        int kind = m->entidades[i].kind;
        if (kind == PM_KIND_NODO || kind == PM_KIND_SCRIPT ||
            kind == PM_KIND_BINDING) return 1;
    }
    return 0;
}

static int mig_detectar(const char *ruta, char *version, size_t tam) {
    FILE *f = fopen(ruta, "r");
    if (!f) return 0;
    version[0] = '\0';
    if (!fgets(version, (int)tam, f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Entrada publica                                                     */
/* ------------------------------------------------------------------ */

int migrar_visual(const char *entrada, const char *salida,
                  const char *copia, const char *informe,
                  char *diagnostico, size_t longitud) {
    Migracion c;
    char copia_auto[MIG_BUF_RUTA];
    char version[256];
    char error[PM_ERROR_MAX];
    ProjectModel *origen = NULL;
    Proyecto3 proyecto;
    int cargado = 0, ok = 0;
    int jerarquico = 0;

    memset(&c, 0, sizeof(c));
    error[0] = '\0';
    if (diagnostico && longitud > 0) diagnostico[0] = '\0';
    if (!entrada || !*entrada || !salida || !*salida) {
        mig_copiar(diagnostico, longitud, "faltan rutas de entrada o salida");
        return 0;
    }
    if (strcmp(entrada, salida) == 0) {
        mig_copiar(diagnostico, longitud, "la salida no puede ser la entrada");
        return 0;
    }
    if (!copia || !*copia) {
        snprintf(copia_auto, sizeof(copia_auto), "%s.bak", entrada);
        copia = copia_auto;
    }
    if (strcmp(copia, salida) == 0) {
        mig_copiar(diagnostico, longitud, "la copia no puede ser la salida");
        return 0;
    }
    if (!mig_copiar_archivo(entrada, copia)) {
        mig_copiar(diagnostico, longitud, "no se pudo crear la copia de seguridad");
        return 0;
    }
    mig_base_ruta(salida, c.base, sizeof(c.base));
    mig_anotar(&c, "migracion entrada=%s salida=%s copia=%s",
               entrada, salida, copia);

    if (!mig_detectar(entrada, version, sizeof(version))) {
        mig_copiar(error, sizeof(error), "no se pudo leer la entrada");
        goto fin;
    }
    if (strncmp(version, "LOG 2", 5) == 0) {
        origen = ml_leer(entrada, error, sizeof(error));
        if (!origen) goto fin;
        jerarquico = mig_es_jerarquico(origen);
        mig_anotar(&c, "origen=LOG2%s", jerarquico ? "_jerarquico" : "");
    } else if (strncmp(version, "LOGIC3", 6) == 0) {
        logic3_iniciar(&proyecto);
        if (!logic3_cargar(&proyecto, entrada, error, sizeof(error))) {
            logic3_liberar(&proyecto);
            goto fin;
        }
        origen = mig_modelo_desde_logic3(&proyecto, &c, error, sizeof(error));
        logic3_liberar(&proyecto);
        if (!origen) goto fin;
        mig_anotar(&c, "origen=LOGIC3_bloques_sistema");
    } else {
        mig_copiar(error, sizeof(error), "formato de entrada no soportado");
        goto fin;
    }

    ok = mig_convertir_modelo(origen, &c, salida, error, sizeof(error),
                              jerarquico);
    if (origen) pm_destruir(origen);
    cargado = 1;

fin:
    if (informe && *informe)
        mig_escribir_texto(informe, c.informe);
    mig_copiar(diagnostico, longitud, ok ? "sin errores" : error);
    (void)cargado;
    return ok;
}
