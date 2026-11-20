/* ==========================================================================
 * Fase 35: editor de variables, listas, trajes y sonidos.
 *
 * Variables, listas y trajes viven en los campos que ya tiene LOGIC3, asi
 * que el escritor canonico y el CRC no se tocan. Las observaciones de
 * variables y la tabla de sonidos se guardan en un archivo lateral
 * `<proyecto>.fase35`, porque el formato LOGIC3 no tiene campos para ellos.
 * Todo el estado nuevo es de editor y no se escribe en el LOGIC3.
 * ========================================================================== */
#include "app.h"
#include "logic3.h"
#include <ctype.h>

#define DATOS_FILA_ALTO      18
#define DATOS_LISTA_Y        (AREA_SUP_ALTO + 74)
#define DATOS_COLOR_TEXTO    0x00D8E0E8u
#define DATOS_COLOR_BARRA    0x001E88E5u
#define DATOS_COLOR_FONDO    0x00141E2Cu
#define DATOS_COLOR_SEL      0x002278C8u
#define DATOS_COLOR_BORDE    0x00304860u
#define DATOS_COLOR_AVISO    0x00E0A020u

typedef struct {
    char clave[DATOS_CLAVE_MAX];
    char texto[DATOS_OBS_TEXTO_MAX];
} ObservacionLocal;

static ObservacionLocal observaciones[DATOS_OBS_MAX];
static int num_observaciones;
static SonidoDatos sonidos[DATOS_SONIDOS_MAX];
static int num_sonidos;
static char ruta_proyecto[MAX_PATH];
static char ultimo_error[192];
static int inicializado;
static int seccion;       /* 0 globales, 1 objeto, 2 listas, 3 sonidos */
static int seleccion;
static int celda;
static int foco;          /* 0 ninguno, 1 nombre, 2 valor, 3 observacion */
static int confirmar;     /* -1 ninguno; codigo tipo*1000+indice */
static int scroll_datos;
static int refs_sustituidas;

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */
static void asegurar(void) {
    if (inicializado) return;
    memset(observaciones, 0, sizeof(observaciones));
    memset(sonidos, 0, sizeof(sonidos));
    num_observaciones = 0;
    num_sonidos = 0;
    ruta_proyecto[0] = 0;
    ultimo_error[0] = 0;
    seccion = 0;
    seleccion = 0;
    celda = 0;
    foco = 0;
    confirmar = -1;
    scroll_datos = 0;
    refs_sustituidas = 0;
    inicializado = 1;
    if (g.estudio_ruta[0]) recortar_texto(ruta_proyecto, g.estudio_ruta, MAX_PATH);
}

static int indice_objeto(const Proyecto3 *p, int id) {
    int i;
    if (!p) return -1;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id == id) return i;
    return -1;
}

static void error_datos(const char *formato, ...) {
    va_list args;
    va_start(args, formato);
    _vsnprintf(ultimo_error, sizeof(ultimo_error) - 1, formato, args);
    va_end(args);
    ultimo_error[sizeof(ultimo_error) - 1] = 0;
    app_estado("%s", ultimo_error);
}

const char *datos_ultimo_error(void) {
    asegurar();
    return ultimo_error;
}

int datos_refs_sustituidas(void) {
    asegurar();
    return refs_sustituidas;
}

static void sanitizar(char *dst, int tam, const char *src) {
    int i;
    if (!src) src = "";
    for (i = 0; src[i] && i < tam - 1; i++) {
        unsigned char ch = (unsigned char)src[i];
        if (ch == '|' || ch == '\n' || ch == '\r' || ch == '\t' || ch < 32)
            dst[i] = '_';
        else
            dst[i] = (char)ch;
    }
    dst[i] = 0;
}

/* Nombre con extension: se usa para copiar assets y para las rutas relativas
 * que guarda LOGIC3. */
static void nombre_archivo(char *dst, int tam, const char *ruta) {
    char nombre[MAX_PATH];
    ruta_nombre(nombre, sizeof(nombre), ruta);
    sanitizar(dst, tam, nombre);
}

/* Nombre sin extension: se usa solo como etiqueta visible en la interfaz. */
static void nombre_base(char *dst, int tam, const char *ruta) {
    char nombre[MAX_PATH];
    char *punto;
    ruta_nombre(nombre, sizeof(nombre), ruta);
    punto = strrchr(nombre, '.');
    if (punto && punto != nombre) *punto = 0;
    sanitizar(dst, tam, nombre);
}

static void ruta_sidecar(char *dst, int tam, const char *ruta) {
    char base[MAX_PATH];
    int n;
    if (!ruta || !ruta[0]) ruta = ruta_proyecto;
    if (!ruta || !ruta[0]) ruta = g.estudio_ruta;
    if (!ruta || !ruta[0]) { dst[0] = 0; return; }
    recortar_texto(base, ruta, sizeof(base));
    n = (int)strlen(base);
    while (n > 0 && base[n - 1] != '.' && base[n - 1] != '\\' &&
           base[n - 1] != '/') n--;
    if (n > 0 && base[n - 1] == '.') base[n - 1] = 0;
    recortar_texto(dst, base, tam);
    strncat(dst, ".fase35", tam - strlen(dst) - 1);
}

static void ruta_asset(char *dst, int tam, const char *carpeta,
                       const char *nombre) {
    char directorio[MAX_PATH];
    char archivo[MAX_PATH];
    char destino[MAX_PATH];
    if (ruta_proyecto[0])
        ruta_carpeta(directorio, sizeof(directorio), ruta_proyecto);
    else if (g.estudio_ruta[0])
        ruta_carpeta(directorio, sizeof(directorio), g.estudio_ruta);
    else
        directorio[0] = 0;
    unir_ruta(destino, sizeof(destino), directorio, carpeta);
    crear_carpeta(destino);
    nombre_archivo(archivo, sizeof(archivo), nombre);
    unir_ruta(dst, tam, destino, archivo);
}

/* Divide una linea por '|' sin modifyar la original. */
static int dividir_linea(char *linea, char **campos, int max) {
    int n = 1;
    campos[0] = linea;
    while (*linea && n < max) {
        if (*linea == '|') {
            *linea = 0;
            campos[n++] = linea + 1;
        }
        linea++;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Observaciones                                                       */
/* ------------------------------------------------------------------ */
static void clave_variable(int sec, int indice, char *dst, int tam) {
    Proyecto3 *p = logic3_modelo_activo();
    if (!p) { dst[0] = 0; return; }
    if (sec == 0 && indice >= 0 && indice < p->num_variables)
        formatear(dst, tam, "G:%s", p->variables[indice].nombre);
    else if (sec == 1 && indice >= 0) {
        int oi = indice_objeto(p, g.estudio_objeto);
        int id = oi >= 0 ? p->objetos[oi].id : 0;
        if (oi >= 0 && indice < p->objetos[oi].num_variables)
            formatear(dst, tam, "O%d:%s", id,
                      p->objetos[oi].variables[indice].nombre);
        else
            formatear(dst, tam, "O%d:", id);
    } else
        dst[0] = 0;
}

static ObservacionLocal *obs_buscar(const char *clave) {
    int i;
    if (!clave || !clave[0]) return NULL;
    for (i = 0; i < num_observaciones; i++)
        if (strcmp(observaciones[i].clave, clave) == 0)
            return &observaciones[i];
    return NULL;
}

static void obs_poner(const char *clave, const char *texto) {
    ObservacionLocal *o;
    if (!clave || !clave[0]) return;
    o = obs_buscar(clave);
    if (!o) {
        if (num_observaciones >= DATOS_OBS_MAX) return;
        o = &observaciones[num_observaciones++];
        memset(o, 0, sizeof(*o));
        sanitizar(o->clave, sizeof(o->clave), clave);
    }
    sanitizar(o->texto, sizeof(o->texto), texto);
}

static void obs_renombrar(const char *vieja, const char *nueva) {
    ObservacionLocal *o = obs_buscar(vieja);
    if (!o || !nueva) return;
    sanitizar(o->clave, sizeof(o->clave), nueva);
}

static const char *obs_obtener(const char *clave) {
    ObservacionLocal *o = obs_buscar(clave);
    return o ? o->texto : "";
}

/* ------------------------------------------------------------------ */
/* Mutaciones del modelo                                               */
/* ------------------------------------------------------------------ */
void datos_proyecto_cambiar(const char *ruta) {
    asegurar();
    if (ruta) recortar_texto(ruta_proyecto, ruta, MAX_PATH);
    else ruta_proyecto[0] = 0;
    num_observaciones = 0;
    num_sonidos = 0;
    seccion = 0;
    seleccion = 0;
    celda = 0;
    foco = 0;
    confirmar = -1;
    scroll_datos = 0;
    refs_sustituidas = 0;
    ultimo_error[0] = 0;
}

int datos_variable_nueva(int objeto_id, const char *nombre, int tipo,
                         const char *valor, const char *obs) {
    Proyecto3 *p = logic3_modelo_activo();
    Variable3 *v = NULL;
    char clave[DATOS_CLAVE_MAX];
    int max, n;
    asegurar();
    if (!p) {
        error_datos("Abre o crea un proyecto antes de crear variables");
        return 0;
    }
    if (objeto_id > 0) {
        int i = indice_objeto(p, objeto_id);
        if (i < 0) {
            error_datos("El objeto no existe");
            return 0;
        }
        max = L3_VARIABLES_OBJETO_MAX;
        n = p->objetos[i].num_variables;
        if (n < max) v = &p->objetos[i].variables[n];
    } else {
        max = L3_VARIABLES_GLOBAL_MAX;
        n = p->num_variables;
        if (n < max) v = &p->variables[n];
    }
    if (!v || n >= max) {
        error_datos("No caben mas variables");
        return 0;
    }
    if (tipo < L3_TIPO_ENTERO || tipo > L3_TIPO_TEXTO) {
        error_datos("Tipo de variable desconocido");
        return 0;
    }
    memset(v, 0, sizeof(*v));
    sanitizar(v->nombre, sizeof(v->nombre),
              nombre && nombre[0] ? nombre : "variable");
    v->tipo = tipo;
    sanitizar(v->valor, sizeof(v->valor), valor ? valor : "");
    v->linea = 0;
    if (objeto_id > 0) p->objetos[indice_objeto(p, objeto_id)].num_variables++;
    else p->num_variables++;
    clave_variable(objeto_id > 0 ? 1 : 0, n, clave, sizeof(clave));
    if (obs) obs_poner(clave, obs);
    seccion = objeto_id > 0 ? 1 : 0;
    seleccion = n;
    foco = 0;
    app_invalidar();
    return 1;
}

/* `elementos_txt` usa ';' como separador. Los valores con '-' se dejan
 * tal cual para que el escritor canonico decida como citarlos. */
int datos_lista_nueva(const char *nombre, int capacidad, int tipo,
                      const char *elementos_txt) {
    Proyecto3 *p = logic3_modelo_activo();
    Lista3 *li;
    char *elementos = NULL;
    char *puntero, *inicio;
    int n = 0;
    asegurar();
    if (!p) {
        error_datos("Abre o crea un proyecto antes de crear listas");
        return 0;
    }
    if (p->num_listas >= L3_LISTAS_MAX) {
        error_datos("No caben mas listas");
        return 0;
    }
    if (capacidad < 0 || capacidad > L3_LISTA_ELEMENTOS_MAX) {
        error_datos("Capacidad de lista fuera de rango");
        return 0;
    }
    if (tipo < L3_TIPO_ENTERO || tipo > L3_TIPO_TEXTO) {
        error_datos("Tipo de elemento desconocido");
        return 0;
    }
    if (elementos_txt && elementos_txt[0]) {
        elementos = (char *)malloc(strlen(elementos_txt) + 1);
        if (!elementos) {
            error_datos("Sin memoria para la lista");
            return 0;
        }
        strcpy(elementos, elementos_txt);
    }
    li = &p->listas[p->num_listas++];
    memset(li, 0, sizeof(*li));
    sanitizar(li->nombre, sizeof(li->nombre), nombre ? nombre : "lista");
    li->capacidad = capacidad;
    li->tipo = tipo;
    li->linea = 0;
    if (elementos) {
        li->elementos = (char (*)[L3_TEXTO_MAX])calloc(
            (size_t)L3_LISTA_ELEMENTOS_MAX, (size_t)L3_TEXTO_MAX);
        if (!li->elementos) {
            error_datos("Sin memoria para los elementos");
            free(elementos);
            return 0;
        }
        inicio = elementos;
        puntero = elementos;
        while (*puntero && n < L3_LISTA_ELEMENTOS_MAX) {
            if (*puntero == ';') {
                *puntero = 0;
                sanitizar(li->elementos[n++], L3_TEXTO_MAX, inicio);
                inicio = puntero + 1;
            }
            puntero++;
        }
        if (*inicio && n < L3_LISTA_ELEMENTOS_MAX)
            sanitizar(li->elementos[n++], L3_TEXTO_MAX, inicio);
        li->num_elementos = n;
        free(elementos);
    }
    seccion = 2;
    seleccion = p->num_listas - 1;
    celda = 0;
    foco = 0;
    app_invalidar();
    return 1;
}

int datos_lista_editar(int indice, int elemento, const char *valor) {
    Proyecto3 *p = logic3_modelo_activo();
    asegurar();
    if (!p || indice < 0 || indice >= p->num_listas) {
        error_datos("La lista no existe");
        return 0;
    }
    if (elemento < 0 || elemento >= p->listas[indice].num_elementos) {
        error_datos("La celda no existe");
        return 0;
    }
    sanitizar(p->listas[indice].elementos[elemento], L3_TEXTO_MAX,
              valor ? valor : "");
    celda = elemento;
    app_invalidar();
    return 1;
}

int datos_traje_importar(const char *ruta, int x, int y, int ancho, int alto,
                         int pivote_x, int pivote_y, int cuadros,
                         int cuadro_ancho, int cuadro_alto) {
    Proyecto3 *p = logic3_modelo_activo();
    char *pixeles = NULL;
    char destino[MAX_PATH];
    char relativo[MAX_PATH];
    char nombre[MAX_PATH];
    int iw = 0, ih = 0, oi, n;
    unsigned char *bytes = NULL;
    int largo = 0;
    Traje3 *t;
    asegurar();
    if (!p) {
        error_datos("Abre o crea un proyecto antes de importar trajes");
        return 0;
    }
    oi = indice_objeto(p, g.estudio_objeto);
    if (oi < 0) {
        error_datos("Selecciona un objeto para importar su traje");
        return 0;
    }
    if (!ruta || !ruta[0] || (!termina_con(ruta, ".png") &&
                              !termina_con(ruta, ".bmp"))) {
        error_datos("El traje debe ser PNG o BMP");
        return 0;
    }
    pixeles = (char *)imagen_cargar(ruta, &iw, &ih);
    if (!pixeles || iw <= 0 || ih <= 0) {
        free(pixeles);
        error_datos("No se pudo leer la imagen del traje");
        return 0;
    }
    free(pixeles);
    if (x < 0 || y < 0 || ancho <= 0 || alto <= 0 ||
        x + ancho > iw || y + alto > ih) {
        error_datos("El recorte no cabe en la imagen");
        return 0;
    }
    if (pivote_x < 0 || pivote_y < 0 || pivote_x > ancho || pivote_y > alto) {
        error_datos("El pivote esta fuera del recorte");
        return 0;
    }
    if (cuadros < 1 || cuadros > 1024 || cuadro_ancho <= 0 ||
        cuadro_alto <= 0 || cuadro_ancho > ancho || cuadro_alto > alto ||
        (cuadros - 1) * cuadro_ancho >= ancho) {
        error_datos("La tira de cuadros no es valida");
        return 0;
    }
    if (p->objetos[oi].num_trajes >= L3_TRAJES_MAX) {
        error_datos("No caben mas trajes");
        return 0;
    }
    if (!leer_archivo(ruta, (char **)&bytes, &largo) || largo <= 0) {
        free(bytes);
        error_datos("No se pudo copiar la imagen");
        return 0;
    }
    nombre_archivo(nombre, sizeof(nombre), ruta);
    ruta_asset(destino, sizeof(destino), "trajes", nombre);
    if (!escribir_archivo(destino, (const char *)bytes, largo)) {
        free(bytes);
        error_datos("No se pudo escribir el traje");
        return 0;
    }
    free(bytes);
    formatear(relativo, sizeof(relativo), "trajes/%s", nombre);
    n = p->objetos[oi].num_trajes++;
    t = &p->objetos[oi].trajes[n];
    memset(t, 0, sizeof(*t));
    t->indice = n;
    recortar_texto(t->imagen, relativo, sizeof(t->imagen));
    t->ancho = ancho;
    t->alto = alto;
    t->pivote_x = pivote_x;
    t->pivote_y = pivote_y;
    t->cuadros = cuadros;
    t->cuadro_ancho = cuadro_ancho;
    t->cuadro_alto = cuadro_alto;
    t->velocidad = 1;
    seccion = 1;
    seleccion = 0;
    app_invalidar();
    return 1;
}

int datos_sonido_importar(const char *nombre, const char *ruta) {
    char destino[MAX_PATH];
    char relativo[MAX_PATH];
    char archivo[MAX_PATH];
    char *bytes = NULL;
    int largo = 0, hz = 0, ch = 0, bits = 0, tam = 0;
    SonidoDatos *s;
    asegurar();
    if (!sonido_validar_wav(ruta, &hz, &ch, &bits, &tam)) {
        error_datos("WAV no valido: PCM mono 8/16 bits a 11025 o 22050 Hz");
        return 0;
    }
    if (num_sonidos >= DATOS_SONIDOS_MAX) {
        error_datos("No caben mas sonidos");
        return 0;
    }
    if (!leer_archivo(ruta, &bytes, &largo) || largo <= 0) {
        free(bytes);
        error_datos("No se pudo copiar el WAV");
        return 0;
    }
    nombre_archivo(archivo, sizeof(archivo), ruta);
    ruta_asset(destino, sizeof(destino), "sonidos", archivo);
    if (!escribir_archivo(destino, bytes, largo)) {
        free(bytes);
        error_datos("No se pudo escribir el sonido");
        return 0;
    }
    free(bytes);
    formatear(relativo, sizeof(relativo), "sonidos/%s", archivo);
    s = &sonidos[num_sonidos++];
    memset(s, 0, sizeof(*s));
    sanitizar(s->nombre, sizeof(s->nombre), nombre && nombre[0] ? nombre : archivo);
    recortar_texto(s->ruta, relativo, sizeof(s->ruta));
    s->frecuencia = hz;
    s->canales = ch;
    s->bits = bits;
    s->bytes = tam;
    s->valido = 1;
    seccion = 3;
    seleccion = num_sonidos - 1;
    foco = 0;
    app_invalidar();
    return 1;
}

int datos_sonido_probar(int indice) {
    char ruta[MAX_PATH];
    char directorio[MAX_PATH];
    asegurar();
    if (indice < 0 || indice >= num_sonidos) {
        error_datos("El sonido no existe");
        return 0;
    }
    if (ruta_proyecto[0]) ruta_carpeta(directorio, sizeof(directorio), ruta_proyecto);
    else if (g.estudio_ruta[0]) ruta_carpeta(directorio, sizeof(directorio), g.estudio_ruta);
    else directorio[0] = 0;
    unir_ruta(ruta, sizeof(ruta), directorio, sonidos[indice].ruta);
    if (!sonido_reproducir(ruta)) {
        /* Sin dispositivo de audio no es un error de validacion: no debe
         * cambiar el volcado ni el resultado de las pruebas. */
        app_estado("No se pudo reproducir el sonido");
        return 0;
    }
    app_estado("Probando sonido: %s", sonidos[indice].nombre);
    return 1;
}

/* Sustuye el nombre en campos de bloques y en codigo.lobo. Cuenta cada
 * aparicion para que las pruebas puedan comprobar el resultado. */
static void reemplazar_token(char *dst, int tam, const char *src,
                             const char *viejo, const char *nuevo, int *n) {
    int i = 0, j = 0;
    size_t lv = strlen(viejo);
    while (src[i] && j < tam - 1) {
        if (strncmp(src + i, viejo, lv) == 0 &&
            (i == 0 || !isalnum((unsigned char)src[i - 1])) &&
            !isalnum((unsigned char)src[i + lv])) {
            int k;
            for (k = 0; nuevo[k] && j < tam - 1; k++) dst[j++] = nuevo[k];
            i += (int)lv;
            if (n) (*n)++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = 0;
}

int datos_sustituir_referencias(const char *viejo, const char *nuevo) {
    Proyecto3 *p = logic3_modelo_activo();
    char tmp[FUNCIONES_CODIGO_MAX];
    int i, j, antes;
    asegurar();
    if (!p || !viejo || !viejo[0] || !nuevo || !nuevo[0]) return 0;
    antes = refs_sustituidas;
    for (i = 0; i < p->num_bloques; i++) {
        Bloque3 *b = &p->bloques[i];
        for (j = 0; j < b->num_campos; j++) {
            if (strcmp(b->campos[j].valor, viejo) == 0) {
                recortar_texto(b->campos[j].valor, nuevo,
                               sizeof(b->campos[j].valor));
                refs_sustituidas++;
            }
        }
        if (b->tiene_codigo && b->codigo[0]) {
            reemplazar_token(tmp, sizeof(tmp), b->codigo, viejo, nuevo,
                             &refs_sustituidas);
            recortar_texto(b->codigo, tmp, sizeof(b->codigo));
        }
    }
    app_invalidar();
    return refs_sustituidas - antes;
}

int datos_borrar(int tipo, int indice, int confirmado, const char *reemplazo) {
    Proyecto3 *p = logic3_modelo_activo();
    char viejo[DATOS_CLAVE_MAX] = "";
    char clave[DATOS_CLAVE_MAX];
    asegurar();
    if (confirmar < 0 && !confirmado) {
        confirmar = (tipo + 1) * 1000 + indice;
        app_estado("Pulsa S para confirmar el borrado");
        return 0;
    }
    confirmar = -1;
    if (!p) return 0;
    if (tipo == 0) {
        if (indice < 0 || indice >= p->num_variables) return 0;
        recortar_texto(viejo, p->variables[indice].nombre, sizeof(viejo));
        clave_variable(0, indice, clave, sizeof(clave));
        {
            ObservacionLocal *o = obs_buscar(clave);
            if (o) {
                *o = observaciones[--num_observaciones];
                memset(&observaciones[num_observaciones], 0,
                       sizeof(observaciones[num_observaciones]));
            }
        }
        memmove(&p->variables[indice], &p->variables[indice + 1],
                sizeof(Variable3) * (size_t)(p->num_variables - indice - 1));
        p->num_variables--;
    } else if (tipo == 1) {
        int oi = indice_objeto(p, g.estudio_objeto);
        if (oi < 0 || indice < 0 || indice >= p->objetos[oi].num_variables)
            return 0;
        recortar_texto(viejo, p->objetos[oi].variables[indice].nombre,
                       sizeof(viejo));
        clave_variable(1, indice, clave, sizeof(clave));
        {
            ObservacionLocal *o = obs_buscar(clave);
            if (o) {
                *o = observaciones[--num_observaciones];
                memset(&observaciones[num_observaciones], 0,
                       sizeof(observaciones[num_observaciones]));
            }
        }
        memmove(&p->objetos[oi].variables[indice],
                &p->objetos[oi].variables[indice + 1],
                sizeof(Variable3) *
                (size_t)(p->objetos[oi].num_variables - indice - 1));
        p->objetos[oi].num_variables--;
    } else if (tipo == 2) {
        if (indice < 0 || indice >= p->num_listas) return 0;
        free(p->listas[indice].elementos);
        p->listas[indice].elementos = NULL;
        memmove(&p->listas[indice], &p->listas[indice + 1],
                sizeof(Lista3) * (size_t)(p->num_listas - indice - 1));
        memset(&p->listas[p->num_listas - 1], 0, sizeof(Lista3));
        p->num_listas--;
    } else if (tipo == 3) {
        if (indice < 0 || indice >= num_sonidos) return 0;
        memmove(&sonidos[indice], &sonidos[indice + 1],
                sizeof(SonidoDatos) * (size_t)(num_sonidos - indice - 1));
        memset(&sonidos[--num_sonidos], 0, sizeof(SonidoDatos));
    } else if (tipo == 4) {
        int oi = indice_objeto(p, g.estudio_objeto);
        if (oi < 0 || indice < 0 || indice >= p->objetos[oi].num_trajes)
            return 0;
        memmove(&p->objetos[oi].trajes[indice],
                &p->objetos[oi].trajes[indice + 1],
                sizeof(Traje3) *
                (size_t)(p->objetos[oi].num_trajes - indice - 1));
        p->objetos[oi].num_trajes--;
    } else {
        return 0;
    }
    if (viejo[0] && reemplazo && reemplazo[0])
        datos_sustituir_referencias(viejo, reemplazo);
    if (seleccion > 0) seleccion--;
    if (seleccion < 0) seleccion = 0;
    foco = 0;
    app_invalidar();
    app_estado("Elemento borrado");
    return 1;
}

/* ------------------------------------------------------------------ */
/* Serializacion lateral y migracion                                   */
/* ------------------------------------------------------------------ */
int datos_cargar(const char *ruta) {
    char ruta_archivo[MAX_PATH];
    FILE *f;
    char linea[512];
    asegurar();
    ruta_sidecar(ruta_archivo, sizeof(ruta_archivo), ruta);
    if (!ruta_archivo[0]) return 0;
    f = fopen(ruta_archivo, "r");
    if (!f) return 0;
    num_observaciones = 0;
    num_sonidos = 0;
    while (fgets(linea, sizeof(linea), f)) {
        char *campos[8];
        char *salto = strchr(linea, '\n');
        int n;
        if (salto) *salto = 0;
        salto = strchr(linea, '\r');
        if (salto) *salto = 0;
        if (!linea[0] || linea[0] == '#') continue;
        n = dividir_linea(linea, campos, 8);
        if (strcmp(campos[0], "observacion") == 0 && n >= 3) {
            if (num_observaciones < DATOS_OBS_MAX)
                obs_poner(campos[1], campos[2]);
        } else if (strcmp(campos[0], "sonido") == 0 && n >= 6) {
            SonidoDatos *s;
            if (num_sonidos >= DATOS_SONIDOS_MAX) continue;
            s = &sonidos[num_sonidos++];
            memset(s, 0, sizeof(*s));
            sanitizar(s->nombre, sizeof(s->nombre), campos[1]);
            recortar_texto(s->ruta, campos[2], sizeof(s->ruta));
            s->frecuencia = atoi(campos[3]);
            s->canales = atoi(campos[4]);
            s->bits = atoi(campos[5]);
            s->bytes = n >= 7 ? atoi(campos[6]) : 0;
            s->valido = 1;
        }
    }
    fclose(f);
    return 1;
}

int datos_guardar(const char *ruta) {
    char ruta_archivo[MAX_PATH];
    FILE *f;
    int i;
    asegurar();
    ruta_sidecar(ruta_archivo, sizeof(ruta_archivo), ruta);
    if (!ruta_archivo[0]) {
        error_datos("No hay ruta de proyecto para guardar los datos");
        return 0;
    }
    f = fopen(ruta_archivo, "w");
    if (!f) {
        error_datos("No se pudo escribir el archivo de datos");
        return 0;
    }
    fprintf(f, "fase35 1\n");
    for (i = 0; i < num_observaciones; i++)
        fprintf(f, "observacion %s|%s\n", observaciones[i].clave,
                observaciones[i].texto);
    for (i = 0; i < num_sonidos; i++)
        fprintf(f, "sonido %s|%s|%d|%d|%d|%d\n", sonidos[i].nombre,
                sonidos[i].ruta, sonidos[i].frecuencia, sonidos[i].canales,
                sonidos[i].bits, sonidos[i].bytes);
    fclose(f);
    /* Sin la ruta: las capturas de pruebas deben ser identicas en cualquier
     * directorio temporal. */
    app_estado("Datos guardados");
    return 1;
}

/* Los proyectos antiguos pueden traer lineas `sonido id "ruta" hz` que el
 * lector LOGIC3 ignora. Se migran al archivo lateral sin tocar el LOGIC3. */
int datos_migrar_sonidos(const char *ruta) {
    FILE *f;
    char linea[512];
    int anadidos = 0;
    asegurar();
    if (!ruta || !ruta[0]) return 0;
    f = fopen(ruta, "r");
    if (!f) return 0;
    while (fgets(linea, sizeof(linea), f)) {
        char *p = linea;
        char *inicio, *fin;
        char ruta_wav[MAX_PATH];
        char nombre[MAX_PATH];
        char archivo[MAX_PATH];
        int hz = 22050, j;
        if (strncmp(p, "sonido ", 7) != 0) continue;
        p += 7;
        while (*p >= '0' && *p <= '9') p++;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '"') continue;
        inicio = p + 1;
        fin = strchr(inicio, '"');
        if (!fin || fin == inicio) continue;
        j = 0;
        while (inicio + j < fin && j < MAX_PATH - 1) {
            ruta_wav[j] = inicio[j];
            j++;
        }
        ruta_wav[j] = 0;
        hz = atoi(fin + 1);
        if (hz != 11025 && hz != 22050) hz = 22050;
        nombre_archivo(archivo, sizeof(archivo), ruta_wav);
        nombre_base(nombre, sizeof(nombre), ruta_wav);
        if (num_sonidos >= DATOS_SONIDOS_MAX) break;
        {
            SonidoDatos *s = &sonidos[num_sonidos++];
            memset(s, 0, sizeof(*s));
            recortar_texto(s->nombre, nombre, sizeof(s->nombre));
            formatear(s->ruta, sizeof(s->ruta), "sonidos/%s", archivo);
            s->frecuencia = hz;
            s->canales = 1;
            s->bits = 16;
            s->valido = 1;
        }
        anadidos++;
    }
    fclose(f);
    if (anadidos) {
        seccion = 3;
        seleccion = num_sonidos - 1;
        app_invalidar();
        app_estado("Sonidos migrados: %d", anadidos);
    }
    return anadidos;
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */
static void dibujar_boton(Zona z, const char *texto, int activo) {
    dibujar_rect_z(z, activo ? DATOS_COLOR_SEL : DATOS_COLOR_BARRA);
    dibujar_marco_z(z, DATOS_COLOR_BORDE);
    fuente_dibujar(z.x + 4, z.y + 5, texto, 0x00FFFFFFu);
}

static int filas_totales(int pestana) {
    Proyecto3 *p = logic3_modelo_activo();
    if (pestana == SONIDOS_TAB) return num_sonidos;
    if (!p) return 0;
    return p->num_variables + (g.estudio_objeto > 0 ? 1 : 0) + p->num_listas;
}

static int fila_info(int fila, int *sec, int *indice) {
    Proyecto3 *p = logic3_modelo_activo();
    int n;
    if (!p) return 0;
    if (fila < p->num_variables) {
        if (sec) *sec = 0;
        if (indice) *indice = fila;
        return 1;
    }
    fila -= p->num_variables;
    if (g.estudio_objeto > 0) {
        int oi = indice_objeto(p, g.estudio_objeto);
        n = oi >= 0 ? p->objetos[oi].num_variables : 0;
        if (fila < n) {
            if (sec) *sec = 1;
            if (indice) *indice = fila;
            return 1;
        }
        fila -= n;
    }
    if (fila < p->num_listas) {
        if (sec) *sec = 2;
        if (indice) *indice = fila;
        return 1;
    }
    return 0;
}

static void dibujar_izquierda(int pestana) {
    Zona izq = estudio_panel_izq_zona();
    const char *botones[4];
    int y = DATOS_LISTA_Y;
    int total, i, sec = -1, ind = -1;
    if (pestana == SONIDOS_TAB) {
        botones[0] = "IMPORT. WAV";
        botones[1] = "PROBAR";
        botones[2] = "MIGRAR";
        botones[3] = "BORRAR";
    } else {
        botones[0] = "NUEVA VAR";
        botones[1] = "NUEVA LISTA";
        botones[2] = "IMP. TRAJE";
        botones[3] = "BORRAR";
    }
    for (i = 0; i < 4; i++)
        dibujar_boton(estudio_boton_zona(i), botones[i],
                      g.estudio_boton_pulsado == i);
    total = filas_totales(pestana);
    for (i = 0; i < total; i++) {
        char texto[64];
        Proyecto3 *p = logic3_modelo_activo();
        int elegido = 0;
        if (pestana == SONIDOS_TAB) {
            elegido = seccion == 3 && seleccion == i;
            formatear(texto, sizeof(texto), "S %s", sonidos[i].nombre);
        } else if (fila_info(i, &sec, &ind) && p) {
            elegido = seccion == sec && seleccion == ind;
            if (sec == 0)
                formatear(texto, sizeof(texto), "G %s", p->variables[ind].nombre);
            else if (sec == 1) {
                int oi = indice_objeto(p, g.estudio_objeto);
                formatear(texto, sizeof(texto), "O %s",
                          oi >= 0 && ind < p->objetos[oi].num_variables
                          ? p->objetos[oi].variables[ind].nombre : "?");
            } else {
                formatear(texto, sizeof(texto), "L %s (%d)",
                          p->listas[ind].nombre, p->listas[ind].num_elementos);
            }
        } else {
            continue;
        }
        if (elegido)
            dibujar_rect(izq.x + 2, y, zona_derecha(izq) - 3,
                         y + DATOS_FILA_ALTO, DATOS_COLOR_SEL);
        fuente_dibujar(izq.x + 6, y + 4, texto,
                       elegido ? 0x00FFFFFFu : DATOS_COLOR_TEXTO);
        y += DATOS_FILA_ALTO;
    }
}

static void campo(int x, int y, int ancho, const char *etiqueta,
                  const char *valor, int enfocado) {
    char linea[128];
    dibujar_rect(x, y, x + ancho, y + 15, DATOS_COLOR_FONDO);
    dibujar_marco(x, y, x + ancho, y + 16,
                  enfocado ? DATOS_COLOR_AVISO : DATOS_COLOR_BORDE);
    formatear(linea, sizeof(linea), "%s: %s", etiqueta, valor);
    fuente_dibujar(x + 4, y + 4, linea,
                   enfocado ? DATOS_COLOR_AVISO : DATOS_COLOR_TEXTO);
}

static void dibujar_centro_datos(Zona centro) {
    Proyecto3 *p = logic3_modelo_activo();
    int x = centro.x + 12;
    int y = centro.y + 52;
    int w = centro.ancho - 24;
    char clave[DATOS_CLAVE_MAX];
    char valor[128];
    int i;
    if (!p) {
        fuente_dibujar(x, y, "ABRE O CREA UN PROYECTO LOGIC3", DATOS_COLOR_AVISO);
        return;
    }
    if (seccion == 2 && seleccion >= 0 && seleccion < p->num_listas) {
        Lista3 *li = &p->listas[seleccion];
        formatear(valor, sizeof(valor), "LISTA %s  TIPO %s  CAP %d/%d",
                  li->nombre, logic3_tipo_texto(li->tipo), li->num_elementos,
                  li->capacidad);
        fuente_dibujar(x, y, valor, DATOS_COLOR_BARRA);
        y += 24;
        for (i = 0; i < li->num_elementos && i < 20; i++) {
            char fila[80];
            int yy = y + i * 16;
            formatear(fila, sizeof(fila), "%2d  %s", i, li->elementos[i]);
            dibujar_rect(x, yy, x + w, yy + 14,
                         celda == i ? DATOS_COLOR_SEL : DATOS_COLOR_FONDO);
            fuente_dibujar(x + 4, yy + 3, fila,
                           celda == i ? 0x00FFFFFFu : DATOS_COLOR_TEXTO);
        }
        formatear(valor, sizeof(valor), "PULSA UNA CELDA Y ESCRIBE PARA EDITAR");
        fuente_dibujar(x, zona_abajo(centro) - 28, valor, 0x006080A0u);
        return;
    }
    if (seccion == 0 && seleccion >= 0 && seleccion < p->num_variables) {
        Variable3 *v = &p->variables[seleccion];
        clave_variable(0, seleccion, clave, sizeof(clave));
        formatear(valor, sizeof(valor), "VARIABLE GLOBAL %d", seleccion);
        fuente_dibujar(x, y, valor, DATOS_COLOR_BARRA);
        campo(x, y + 24, w, "nombre", v->nombre, foco == 1);
        campo(x, y + 46, w, "tipo", logic3_tipo_texto(v->tipo), 0);
        campo(x, y + 68, w, "valor", v->valor, foco == 2);
        campo(x, y + 90, w, "observacion", obs_obtener(clave), foco == 3);
        return;
    }
    if (seccion == 1) {
        int oi = indice_objeto(p, g.estudio_objeto);
        if (oi >= 0 && seleccion >= 0 &&
            seleccion < p->objetos[oi].num_variables) {
            Variable3 *v = &p->objetos[oi].variables[seleccion];
            clave_variable(1, seleccion, clave, sizeof(clave));
            formatear(valor, sizeof(valor), "VARIABLE DE OBJETO %d", seleccion);
            fuente_dibujar(x, y, valor, DATOS_COLOR_BARRA);
            campo(x, y + 24, w, "nombre", v->nombre, foco == 1);
            campo(x, y + 46, w, "tipo", logic3_tipo_texto(v->tipo), 0);
            campo(x, y + 68, w, "valor", v->valor, foco == 2);
            campo(x, y + 90, w, "observacion", obs_obtener(clave), foco == 3);
            return;
        }
    }
    fuente_dibujar(x, y, "SELECCIONA UNA VARIABLE O UNA LISTA", DATOS_COLOR_AVISO);
}

static void dibujar_centro_sonidos(Zona centro) {
    int x = centro.x + 12;
    int y = centro.y + 52;
    int w = centro.ancho - 24;
    char linea[160];
    SonidoDatos *s;
    if (seccion != 3 || seleccion < 0 || seleccion >= num_sonidos) {
        fuente_dibujar(x, y, "IMPORTA UN WAV PARA EMPEZAR", DATOS_COLOR_AVISO);
        return;
    }
    s = &sonidos[seleccion];
    formatear(linea, sizeof(linea), "SONIDO %s", s->nombre);
    fuente_dibujar(x, y, linea, DATOS_COLOR_BARRA);
    campo(x, y + 24, w, "archivo", s->ruta, 0);
    formatear(linea, sizeof(linea), "frecuencia %d Hz  canales %d  bits %d",
              s->frecuencia, s->canales, s->bits);
    fuente_dibujar(x, y + 50, linea, DATOS_COLOR_TEXTO);
    formatear(linea, sizeof(linea), "datos %d bytes  valido %s",
              s->bytes, s->valido ? "si" : "no");
    fuente_dibujar(x, y + 68, linea, DATOS_COLOR_TEXTO);
    {
        int barras = w / 8;
        int k;
        for (k = 0; k < barras; k++) {
            int alto = 2 + ((k * 7 + seleccion * 3) % 18);
            dibujar_rect(x + k * 8, y + 110 - alto, x + k * 8 + 5,
                         y + 110, 0x002E7AB8u);
        }
    }
}

static void dibujar_derecha(int pestana, Zona der) {
    int x = der.x + 10;
    int y = der.y + 42;
    fuente_dibujar(x, y, "FASE 35 - DATOS Y ASSETS", DATOS_COLOR_BARRA);
    y += 22;
    if (pestana == SONIDOS_TAB) {
        fuente_dibujar(x, y, "WAV PCM MONO 8/16 BITS", DATOS_COLOR_TEXTO);
        y += 16;
        fuente_dibujar(x, y, "11025 O 22050 HZ", DATOS_COLOR_TEXTO);
        y += 16;
        fuente_dibujar(x, y, "TOPE 256 KB DE DATOS", DATOS_COLOR_TEXTO);
        y += 16;
        fuente_dibujar(x, y, "MIGRAR LEE SONIDOS VIEJOS", DATOS_COLOR_TEXTO);
    } else {
        fuente_dibujar(x, y, "VARIABLES Y LISTAS EN LOGIC3", DATOS_COLOR_TEXTO);
        y += 16;
        fuente_dibujar(x, y, "OBSERVACIONES EN .FASE35", DATOS_COLOR_TEXTO);
        y += 16;
        fuente_dibujar(x, y, "TRAJES: RECORTE Y PIVOTE", DATOS_COLOR_TEXTO);
        y += 16;
        fuente_dibujar(x, y, "BORRAR PIDES CONFIRMACION", DATOS_COLOR_TEXTO);
    }
    if (confirmar >= 0) {
        y = zona_abajo(der) - 44;
        dibujar_rect(x, y, zona_derecha(der) - 10, y + 28, DATOS_COLOR_AVISO);
        fuente_dibujar(x + 4, y + 4, "BORRAR? PULSA S O N",
                       0x00101820u);
    }
}

void datos_dibujar(int pestana) {
    asegurar();
    dibujar_izquierda(pestana);
    if (pestana == SONIDOS_TAB) dibujar_centro_sonidos(estudio_lienzo_zona());
    else dibujar_centro_datos(estudio_lienzo_zona());
    dibujar_derecha(pestana, estudio_panel_der_zona());
}

/* ------------------------------------------------------------------ */
/* Interaccion                                                          */
/* ------------------------------------------------------------------ */
static int dialogo_archivo(char *ruta, const char *filtro,
                           const char *carpeta) {
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ruta[0] = 0;
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = filtro;
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = carpeta;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                OFN_HIDEREADONLY;
    return GetOpenFileNameA(&ofn) ? 1 : 0;
}

static void carpeta_assets(char *dst, int tam, const char *carpeta) {
    char directorio[MAX_PATH];
    if (ruta_proyecto[0]) ruta_carpeta(directorio, sizeof(directorio), ruta_proyecto);
    else if (g.estudio_ruta[0]) ruta_carpeta(directorio, sizeof(directorio), g.estudio_ruta);
    else directorio[0] = 0;
    unir_ruta(dst, tam, directorio, carpeta);
    crear_carpeta(dst);
}

static void importar_traje_ui(void) {
    char ruta[MAX_PATH];
    char carpeta[MAX_PATH];
    carpeta_assets(carpeta, sizeof(carpeta), "trajes");
    if (!dialogo_archivo(ruta,
        "Imagenes\0*.png;*.bmp\0Todos los archivos\0*.*\0\0", carpeta)) return;
    datos_traje_importar(ruta, 0, 0, 32, 32, 16, 16, 1, 32, 32);
}

static void importar_sonido_ui(void) {
    char ruta[MAX_PATH];
    char nombre[MAX_PATH];
    char carpeta[MAX_PATH];
    carpeta_assets(carpeta, sizeof(carpeta), "sonidos");
    if (!dialogo_archivo(ruta, "WAV\0*.wav\0Todos los archivos\0*.*\0\0",
                         carpeta)) return;
    nombre_base(nombre, sizeof(nombre), ruta);
    datos_sonido_importar(nombre, ruta);
}

static void nueva_variable_ui(void) {
    Proyecto3 *p = logic3_modelo_activo();
    char nombre[32];
    asegurar();
    if (!p) { error_datos("Abre o crea un proyecto"); return; }
    formatear(nombre, sizeof(nombre), "var%d",
              seccion == 1 ? p->num_variables + 1 : p->num_variables + 1);
    if (seccion == 1) datos_variable_nueva(g.estudio_objeto, nombre,
                                           L3_TIPO_ENTERO, "0", "");
    else datos_variable_nueva(0, nombre, L3_TIPO_ENTERO, "0", "");
}

static void nueva_lista_ui(void) {
    Proyecto3 *p = logic3_modelo_activo();
    char nombre[32];
    char elementos[32];
    asegurar();
    if (!p) { error_datos("Abre o crea un proyecto"); return; }
    formatear(nombre, sizeof(nombre), "lista%d", p->num_listas + 1);
    elementos[0] = 0;
    datos_lista_nueva(nombre, 10, L3_TIPO_TEXTO, elementos);
}

void datos_pulsar(int x, int y, int boton) {
    Zona izq = estudio_panel_izq_zona();
    Zona centro = estudio_lienzo_zona();
    int i, pestana = g.estudio_pestana;
    Proyecto3 *p;
    asegurar();
    if (boton != 0) return;
    if (zona_contiene(izq, x, y)) {
        for (i = 0; i < 4; i++) {
            if (!zona_contiene(estudio_boton_zona(i), x, y)) continue;
            g.estudio_boton_pulsado = i;
            if (pestana == SONIDOS_TAB) {
                if (i == 0) importar_sonido_ui();
                else if (i == 1 && seccion == 3) datos_sonido_probar(seleccion);
                else if (i == 2) {
                    if (g.estudio_ruta[0]) datos_migrar_sonidos(g.estudio_ruta);
                    else error_datos("Guarda el proyecto antes de migrar");
                } else if (i == 3 && seccion == 3)
                    datos_borrar(3, seleccion, 0, NULL);
            } else {
                if (i == 0) nueva_variable_ui();
                else if (i == 1) nueva_lista_ui();
                else if (i == 2) importar_traje_ui();
                else if (i == 3) datos_borrar(seccion, seleccion, 0, NULL);
            }
            app_invalidar();
            return;
        }
        if (y >= DATOS_LISTA_Y) {
            int fila = (y - DATOS_LISTA_Y) / DATOS_FILA_ALTO;
            if (pestana == SONIDOS_TAB) {
                if (fila >= 0 && fila < num_sonidos) {
                    seccion = 3;
                    seleccion = fila;
                    foco = 0;
                }
            } else {
                int sec = -1, ind = -1;
                if (fila_info(fila, &sec, &ind)) {
                    seccion = sec;
                    seleccion = ind;
                    celda = 0;
                    foco = 0;
                }
            }
            app_invalidar();
        }
        return;
    }
    if (pestana != SONIDOS_TAB && seccion == 2 &&
        zona_contiene(centro, x, y)) {
        p = logic3_modelo_activo();
        if (p && seleccion >= 0 && seleccion < p->num_listas) {
            int cy = centro.y + 52 + 24;
            int fila = (y - cy) / 16;
            if (fila >= 0 && fila < p->listas[seleccion].num_elementos) {
                celda = fila;
                foco = 2;
                app_invalidar();
            }
        }
        return;
    }
    if (pestana != SONIDOS_TAB && zona_contiene(centro, x, y)) {
        int cx = centro.x + 12;
        int cy = centro.y + 52 + 24;
        if (y >= cy && y < cy + 4 * 22) {
            int campo_idx = (y - cy) / 22;
            if (campo_idx == 0 || campo_idx == 1 || campo_idx == 2 ||
                campo_idx == 3) {
                foco = campo_idx == 3 ? 3 : (campo_idx == 0 ? 1 : 2);
                app_invalidar();
            }
        }
        (void)cx;
    }
}

void datos_soltar(int x, int y, int boton) {
    (void)x;
    (void)y;
    (void)boton;
    if (g.estudio_boton_pulsado >= 0) {
        g.estudio_boton_pulsado = -1;
        app_invalidar();
    }
}

void datos_mover(int x, int y, int botones) {
    Zona izq = estudio_panel_izq_zona();
    Zona barra = estudio_barra_zona();
    int hover = -1, i;
    (void)botones;
    if (zona_contiene(barra, x, y)) {
        for (i = 0; i < ESTUDIO_PESTANAS; i++)
            if (zona_contiene(estudio_barra_boton_zona(i), x, y)) hover = 100 + i;
    } else if (zona_contiene(izq, x, y)) {
        for (i = 0; i < 4; i++)
            if (zona_contiene(estudio_boton_zona(i), x, y)) hover = i;
    }
    if (hover != g.estudio_hover) {
        g.estudio_hover = hover;
        app_invalidar();
    }
}

void datos_rueda(int x, int y, int delta, int ctrl) {
    int total, visibles, maximo;
    (void)ctrl;
    asegurar();
    if (!zona_contiene(estudio_panel_izq_zona(), x, y)) return;
    total = filas_totales(g.estudio_pestana);
    visibles = (g.alto - DATOS_LISTA_Y - 8) / DATOS_FILA_ALTO;
    if (visibles < 1) visibles = 1;
    maximo = total - visibles;
    if (maximo < 0) maximo = 0;
    if (delta > 0) scroll_datos--;
    else if (delta < 0) scroll_datos++;
    if (scroll_datos < 0) scroll_datos = 0;
    if (scroll_datos > maximo) scroll_datos = maximo;
    app_invalidar();
}

void datos_tecla(int vk, int ctrl, int shift) {
    char *dest = NULL;
    Proyecto3 *p;
    (void)ctrl;
    (void)shift;
    asegurar();
    if (confirmar >= 0) {
        if (vk == 'S' || vk == 's') {
            int tipo = confirmar / 1000 - 1;
            int indice = confirmar % 1000;
            datos_borrar(tipo, indice, 1, NULL);
        } else if (vk == 'N' || vk == 'n' || vk == VK_ESCAPE) {
            confirmar = -1;
            app_estado("Borrado cancelado");
            app_invalidar();
        }
        return;
    }
    if (vk == VK_RETURN) {
        foco = 0;
        app_invalidar();
        return;
    }
    if (vk != VK_BACK) return;
    p = logic3_modelo_activo();
    if (!p || foco == 0) return;
    if (seccion == 2 && foco == 2 && seleccion >= 0 &&
        seleccion < p->num_listas && celda >= 0 &&
        celda < p->listas[seleccion].num_elementos)
        dest = p->listas[seleccion].elementos[celda];
    else if (seccion == 0 && seleccion >= 0 && seleccion < p->num_variables) {
        if (foco == 1) dest = p->variables[seleccion].nombre;
        else if (foco == 2) dest = p->variables[seleccion].valor;
        else if (foco == 3) dest = (char *)obs_obtener("");
    } else if (seccion == 1) {
        int oi = indice_objeto(p, g.estudio_objeto);
        if (oi >= 0 && seleccion >= 0 &&
            seleccion < p->objetos[oi].num_variables) {
            if (foco == 1) dest = p->objetos[oi].variables[seleccion].nombre;
            else if (foco == 2) dest = p->objetos[oi].variables[seleccion].valor;
        }
    }
    if (dest && dest[0]) dest[strlen(dest) - 1] = 0;
    if (foco == 3) {
        char clave[DATOS_CLAVE_MAX];
        ObservacionLocal *o;
        clave_variable(seccion, seleccion, clave, sizeof(clave));
        o = obs_buscar(clave);
        if (o && o->texto[0]) o->texto[strlen(o->texto) - 1] = 0;
    }
    app_invalidar();
}

void datos_caracter(int ch) {
    Proyecto3 *p = logic3_modelo_activo();
    char *dest = NULL;
    int max = 0;
    char clave[DATOS_CLAVE_MAX];
    char vieja[DATOS_CLAVE_MAX];
    asegurar();
    if (confirmar >= 0 || foco == 0 || !p || ch < 32 || ch > 126) return;
    if (seccion == 2) {
        if (foco == 2 && seleccion >= 0 && seleccion < p->num_listas &&
            celda >= 0 && celda < p->listas[seleccion].num_elementos) {
            dest = p->listas[seleccion].elementos[celda];
            max = L3_TEXTO_MAX;
        }
    } else if (seccion == 0 && seleccion >= 0 && seleccion < p->num_variables) {
        if (foco == 1) { dest = p->variables[seleccion].nombre; max = L3_NOMBRE_MAX; }
        else if (foco == 2) { dest = p->variables[seleccion].valor; max = L3_TEXTO_MAX; }
        else if (foco == 3) max = 0;
    } else if (seccion == 1) {
        int oi = indice_objeto(p, g.estudio_objeto);
        if (oi >= 0 && seleccion >= 0 &&
            seleccion < p->objetos[oi].num_variables) {
            if (foco == 1) {
                dest = p->objetos[oi].variables[seleccion].nombre;
                max = L3_NOMBRE_MAX;
            } else if (foco == 2) {
                dest = p->objetos[oi].variables[seleccion].valor;
                max = L3_TEXTO_MAX;
            } else if (foco == 3) max = 0;
        }
    }
    if (foco == 3) {
        clave_variable(seccion, seleccion, clave, sizeof(clave));
        obs_poner(clave, "");
    } else if (!dest || max <= 0 || strlen(dest) >= (size_t)(max - 1)) {
        return;
    }
    if (foco == 1) {
        clave_variable(seccion, seleccion, vieja, sizeof(vieja));
    }
    if (foco == 3) {
        ObservacionLocal *o = obs_buscar(clave);
        if (o && strlen(o->texto) < sizeof(o->texto) - 1) {
            size_t n = strlen(o->texto);
            o->texto[n] = (char)ch;
            o->texto[n + 1] = 0;
        }
    } else {
        size_t n = strlen(dest);
        dest[n] = (char)ch;
        dest[n + 1] = 0;
        if (foco == 1) {
            clave_variable(seccion, seleccion, clave, sizeof(clave));
            obs_renombrar(vieja, clave);
        }
    }
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Volcado de pruebas                                                   */
/* ------------------------------------------------------------------ */
static void volcar_sanitizado(FILE *f, const char *texto) {
    char plano[192];
    int i;
    for (i = 0; texto[i] && i < (int)sizeof(plano) - 1; i++) {
        unsigned char ch = (unsigned char)texto[i];
        plano[i] = (ch == '|' || ch == '\n' || ch == '\r' || ch == ' ') ? '_' : (char)ch;
    }
    plano[i] = 0;
    fputs(plano, f);
}

void datos_volcar(const char *ruta) {
    Proyecto3 *p = logic3_modelo_activo();
    FILE *f;
    int i, j;
    asegurar();
    if (!ruta || !ruta[0]) return;
    f = fopen(ruta, "w");
    if (!f) return;
    fprintf(f, "fase35=1 pestana=%d seccion=%d seleccion=%d celda=%d "
               "foco=%d confirmar=%d refs=%d error=",
            g.estudio_pestana, seccion, seleccion, celda, foco, confirmar,
            refs_sustituidas);
    if (ultimo_error[0]) volcar_sanitizado(f, ultimo_error);
    else fputc('-', f);
    fputc('\n', f);
    for (i = 0; i < num_observaciones; i++) {
        fputs("obs=", f);
        volcar_sanitizado(f, observaciones[i].clave);
        fputc('|', f);
        volcar_sanitizado(f, observaciones[i].texto);
        fputc('\n', f);
    }
    if (p) {
        for (i = 0; i < p->num_variables; i++) {
            fprintf(f, "varG=%d|", i);
            volcar_sanitizado(f, p->variables[i].nombre);
            fprintf(f, "|%s|", logic3_tipo_texto(p->variables[i].tipo));
            volcar_sanitizado(f, p->variables[i].valor);
            fputc('\n', f);
        }
        for (i = 0; i < p->num_listas; i++) {
            Lista3 *li = &p->listas[i];
            fprintf(f, "lista=%d|", i);
            volcar_sanitizado(f, li->nombre);
            fprintf(f, "|%d|%s|%d\n", li->capacidad,
                    logic3_tipo_texto(li->tipo), li->num_elementos);
            for (j = 0; j < li->num_elementos; j++) {
                fprintf(f, "elem=%d|%d|", i, j);
                volcar_sanitizado(f, li->elementos[j]);
                fputc('\n', f);
            }
        }
        for (i = 0; i < p->num_objetos; i++) {
            for (j = 0; j < p->objetos[i].num_variables; j++) {
                Variable3 *v = &p->objetos[i].variables[j];
                fprintf(f, "varO=%d|%d|", p->objetos[i].id, j);
                volcar_sanitizado(f, v->nombre);
                fprintf(f, "|%s|", logic3_tipo_texto(v->tipo));
                volcar_sanitizado(f, v->valor);
                fputc('\n', f);
            }
            for (j = 0; j < p->objetos[i].num_trajes; j++) {
                Traje3 *t = &p->objetos[i].trajes[j];
                fprintf(f, "traje=%d|%d|", p->objetos[i].id, j);
                volcar_sanitizado(f, t->imagen);
                fprintf(f, "|%d|%d|%d|%d|%d|%d|%d|%d\n", t->ancho, t->alto,
                        t->pivote_x, t->pivote_y, t->cuadros,
                        t->cuadro_ancho, t->cuadro_alto, t->velocidad);
            }
        }
    }
    for (i = 0; i < num_sonidos; i++) {
        fputs("sonido=", f);
        volcar_sanitizado(f, sonidos[i].nombre);
        fputc('|', f);
        volcar_sanitizado(f, sonidos[i].ruta);
        fprintf(f, "|%d|%d|%d|%d|%d\n", sonidos[i].frecuencia,
                sonidos[i].canales, sonidos[i].bits, sonidos[i].bytes,
                sonidos[i].valido);
    }
    fclose(f);
}

/* Fase 40: herramienta del editor para ver y borrar saves sin tocar los
 * menus (solo se llama desde los flags nuevos --saves-*). */
void datos_saves_herramienta(const char *carpeta, const char *salida,
                             const char *borrar) {
    WIN32_FIND_DATAA datos;
    HANDLE busqueda;
    FILE *f;
    char patron[MAX_PATH];
    char ruta[MAX_PATH];
    int n = 0;
    int borrado = 0;

    if (!salida || !salida[0]) return;
    f = fopen(salida, "wt");
    if (!f) return;
    fprintf(f, "saves: carpeta %s\n", carpeta ? carpeta : "");
    if (borrar && borrar[0]) {
        _snprintf(ruta, sizeof(ruta), "%s\\%s.sav",
                  carpeta ? carpeta : "", borrar);
        ruta[sizeof(ruta) - 1] = '\0';
        borrado = DeleteFileA(ruta) ? 1 : 0;
        fprintf(f, "saves: borrar %s %d\n", borrar, borrado);
    }
    _snprintf(patron, sizeof(patron), "%s\\*.sav", carpeta ? carpeta : "");
    patron[sizeof(patron) - 1] = '\0';
    busqueda = FindFirstFileA(patron, &datos);
    if (busqueda != INVALID_HANDLE_VALUE) {
        do {
            if (datos.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            _snprintf(ruta, sizeof(ruta), "%s\\%s",
                      carpeta ? carpeta : "", datos.cFileName);
            ruta[sizeof(ruta) - 1] = '\0';
            {
                FILE *s = fopen(ruta, "rt");
                char linea[128];
                char clave[64];
                int valor = 0;
                int entradas = 0;
                int valido = 0;
                if (s) {
                    if (fgets(linea, sizeof(linea), s) &&
                        strncmp(linea, "lw2ksave 1", 10) == 0) {
                        valido = 1;
                        while (fgets(linea, sizeof(linea), s)) {
                            if (linea[0] == '\n' || linea[0] == '\r') continue;
                            if (sscanf(linea, "%63s %d", clave, &valor) != 2) {
                                valido = 0;
                                break;
                            }
                            entradas++;
                        }
                    }
                    fclose(s);
                }
                fprintf(f, "saves: save %s %d %d\n", datos.cFileName,
                        entradas, valido);
                n++;
            }
        } while (FindNextFileA(busqueda, &datos));
        FindClose(busqueda);
    }
    fprintf(f, "saves: archivos %d\n", n);
    fclose(f);
}
