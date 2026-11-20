/* eventos.c - planificador de eventos y cola de mensajes (fase 27).
 *
 * Ver eventos.h: reparte los scripts de todos los eventos del proyecto en
 * el orden del capitulo 5.2 del plan maestro. Cada script conserva su
 * propia LoboMaquina, asi que `esperar` solo deja esperando a ese script.
 *
 * Modelo de instancias: en esta fase cada script tiene una sola maquina y
 * se ejecuta contra la primera entidad viva de su objeto (como en la fase
 * 26). El evento `clonar` si recibe la entidad recien creada, que es lo
 * que necesita una instancia nueva para colocarse. La maquina por entidad
 * completa llegara con el render y la fisica por capas.
 *
 * C puro: ni windows.h ni malloc. Lo enlazan game.exe y el editor.
 */

#include <stdio.h>
#include <string.h>
#include "eventos.h"
#include "depurador.h"
#include "entidades.h"

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

static void copiar_texto(char *dst, const char *src, int tam) {
    int i;
    if (tam <= 0) return;
    for (i = 0; i < tam - 1 && src && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static int texto_igual(const char *a, const char *b) {
    int i;
    if (!a || !b) return 0;
    for (i = 0; a[i] && a[i] == b[i]; i++) {}
    return a[i] == b[i];
}

/* Bit de entrada de una tecla de lobo; 0 si el nombre no es una tecla. */
unsigned eventos_entrada(const Eventos *ev, const char *nombre) {
    (void)ev;
    if (!nombre) return 0;
    if (texto_igual(nombre, "izquierda")) return LOBO_TECLA_IZQUIERDA;
    if (texto_igual(nombre, "derecha")) return LOBO_TECLA_DERECHA;
    if (texto_igual(nombre, "arriba")) return LOBO_TECLA_ARRIBA;
    if (texto_igual(nombre, "abajo")) return LOBO_TECLA_ABAJO;
    if (texto_igual(nombre, "saltar")) return LOBO_TECLA_SALTO;
    if (texto_igual(nombre, "cerrar")) return LOBO_TECLA_CERRAR;
    return 0;
}

/* Fase 42: puntos de medida del perfilador. El reloj lo da el huesped; sin
 * reloj (por ejemplo --vm-test) los microsegundos quedan a cero. */
static unsigned long perfil_ahora(const Eventos *ev) {
    return ev->reloj ? ev->reloj(ev->reloj_ctx) : 0;
}

static const char *tipo_texto(int tipo) {
    switch (tipo) {
    case EVENTO_INICIO: return "inicio";
    case EVENTO_FOTOGRAMA: return "fotograma";
    case EVENTO_TECLA: return "tecla";
    case EVENTO_RECIBIR: return "recibir";
    case EVENTO_TOCAR: return "tocar";
    case EVENTO_CADA: return "cada";
    case EVENTO_CLONAR: return "clonar";
    case EVENTO_ESCENA: return "escena";
    default: return "?";
    }
}

/* Primera entidad viva del objeto en la escena actual (0 si no hay). */
static int entidad_de_objeto(int objeto, int escena) {
    int tipo = entidades_tipo_por_objeto(objeto);
    int i;
    if (tipo < 0) return 0;
    for (i = 0; i < ENTIDADES_MAX; i++) {
        Entidad *e = entidad_indice(i);
        if (e && e->usada && e->tipo == tipo && e->escena == escena)
            return e->id;
    }
    return 0;
}

/* Caja de una entidad (ancho y alto con minimo 1). */
static int caja_entidad(const Entidad *e, int *x, int *y, int *w, int *h) {
    if (!e || !e->usada) return 0;
    *x = e->x;
    *y = e->y;
    *w = e->w > 0 ? e->w : 1;
    *h = e->h > 0 ? e->h : 1;
    return 1;
}

static int cajas_tocan(const Entidad *a, const Entidad *b) {
    int ax, ay, aw, ah, bx, by, bw, bh;
    if (!caja_entidad(a, &ax, &ay, &aw, &ah)) return 0;
    if (!caja_entidad(b, &bx, &by, &bw, &bh)) return 0;
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

/* Primera entidad de la escena que toca a `a` y encaja en el destino del
 * evento (objeto concreto, tag o cualquiera). 0 si no hay. */
static int entidad_tocada(const EventoScript *s, const Entidad *a, int escena) {
    int tipo_destino = -1;
    int i;
    if (!a) return 0;
    if (s->objeto_tocar >= 0)
        tipo_destino = entidades_tipo_por_objeto(s->objeto_tocar);
    else if (s->nombre[0])
        tipo_destino = entidades_tipo_por_tag(s->nombre);
    for (i = 0; i < ENTIDADES_MAX; i++) {
        Entidad *b = entidad_indice(i);
        if (!b || !b->usada || b == a || b->escena != escena) continue;
        if (tipo_destino >= 0 && b->tipo != tipo_destino) continue;
        if (cajas_tocan(a, b)) return b->id;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Cola de mensajes                                                    */
/* ------------------------------------------------------------------ */

void eventos_mensaje(Eventos *ev, int origen, const char *destino,
                    const char *texto) {
    EventoMensaje *m;
    int pos;
    if (!ev || !texto || !texto[0]) return;
    if (ev->cola_num >= EVENTOS_COLA_MAX) {
        /* Cola llena: se avisa y se descarta el nuevo, que es el unico
         * que aun no estaba prometido a nadie. */
        ev->avisos++;
        return;
    }
    pos = (ev->cola_ini + ev->cola_num) % EVENTOS_COLA_MAX;
    m = &ev->cola[pos];
    m->origen = origen;
    copiar_texto(m->destino, destino ? destino : "", EVENTOS_TEXTO_MAX);
    copiar_texto(m->texto, texto, EVENTOS_TEXTO_MAX);
    ev->cola_num++;
}

void eventos_enviar_cb(void *ctx, int origen, const char *destino,
                       const char *texto) {
    eventos_mensaje((Eventos *)ctx, origen, destino, texto);
}

void eventos_reiniciar_cb(void *ctx) {
    if (ctx) ((Eventos *)ctx)->reiniciar = 1;
}

/* ------------------------------------------------------------------ */
/* Carga y arranque                                                    */
/* ------------------------------------------------------------------ */

void eventos_iniciar(Eventos *ev, FILE *traza, int detalle) {
    if (!ev) return;
    memset(ev, 0, sizeof(*ev));
    ev->traza = traza;
    ev->detalle = detalle ? 1 : 0;
}

int eventos_cargar(Eventos *ev, const MotorL3 *proy,
                   EventosRemapear remapear, void *ctx) {
    int i;
    if (!ev || !proy) return 0;
    ev->num_scripts = 0;
    ev->errores = 0;
    ev->bloque_error = 0;
    ev->error[0] = '\0';

    for (i = 0; i < proy->num_scripts; i++) {
        const MotorL3Script *s = &proy->scripts[i];
        EventoScript *d;
        char error[LOBO_ERROR_MAX];

        if (ev->num_scripts >= EVENTOS_SCRIPTS_MAX) break;
        d = &ev->scripts[ev->num_scripts];
        memset(d, 0, sizeof(*d));
        lobo_programa_iniciar(&d->programa);
        if (!lobo_compilar(s->codigo, &d->programa, LOBO_MODO_VM, error,
                           sizeof(error))) {
            if (ev->traza)
                fprintf(ev->traza, "vm: error bloque %d %s\n", s->bloque,
                        error);
            ev->errores++;
            if (ev->errores == 1) {
                ev->bloque_error = s->bloque;
                copiar_texto(ev->error, error, (int)sizeof(ev->error));
            }
            lobo_programa_liberar(&d->programa);
            continue;
        }
        lobo_maquina_reiniciar(&d->maquina);
        if (remapear) remapear(ctx, &d->programa);

        d->bloque = s->bloque;
        d->objeto = s->objeto;
        d->evento = s->evento;
        d->orden = s->orden;
        d->capa = s->capa;
        d->tipo = s->tipo;
        copiar_texto(d->nombre, s->nombre, EVENTOS_TEXTO_MAX);
        d->flanco = s->flanco;
        d->objeto_tocar = s->objeto_tocar;
        d->cada = s->cada > 0 ? s->cada : 0;
        d->escena = s->escena;
        d->contador = d->cada;
        d->cooldown = 0;
        ev->num_scripts++;
    }
    return ev->num_scripts;
}

void eventos_liberar(Eventos *ev) {
    int i;
    if (!ev) return;
    for (i = 0; i < ev->num_scripts; i++)
        lobo_programa_liberar(&ev->scripts[i].programa);
    ev->num_scripts = 0;
}

void eventos_escena_iniciar(Eventos *ev, LoboEstado *estado) {
    int i;
    (void)estado;
    if (!ev) return;
    ev->detener_todo = 0;
    ev->reiniciar = 0;
    ev->cerrar = 0;
    ev->iniciado = 0;
    ev->avisado = 0;
    ev->eventos_fotograma = 0;
    ev->cola_ini = 0;
    ev->cola_num = 0;
    for (i = 0; i < ev->num_scripts; i++) {
        EventoScript *s = &ev->scripts[i];
        lobo_maquina_reiniciar(&s->maquina);
        s->contador = s->cada;
        s->cooldown = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Despacho                                                            */
/* ------------------------------------------------------------------ */

/* Apunta un evento en la traza (--eventos). */
static void traza_evento(Eventos *ev, const EventoScript *s, int indice,
                         int ent, const char *nota) {
    if (!ev->traza || !ev->detalle) return;
    fprintf(ev->traza, "evento %d %s obj %d ent %d script %d", ev->fotograma,
            tipo_texto(s->tipo), s->objeto, ent, indice);
    if (nota && nota[0]) fprintf(ev->traza, " %s", nota);
    fprintf(ev->traza, "\n");
}

/* Gasta una unidad del presupuesto del fotograma. Devuelve 0 si ya no
 * queda, con un unico aviso por fotograma. */
static int gastar(Eventos *ev) {
    if (ev->eventos_fotograma >= EVENTOS_PRESUPUESTO) {
        if (!ev->avisado) {
            ev->avisado = 1;
            ev->avisos++;
            if (ev->traza)
                fprintf(ev->traza,
                        "eventos: presupuesto agotado en el fotograma %d\n",
                        ev->fotograma);
        }
        return 0;
    }
    ev->eventos_fotograma++;
    ev->ejecutados++;
    return 1;
}

/* Lanza un evento sobre un script. `repetir` usa lobo_vm_fotograma (los
 * eventos que se repiten cada fotograma); si no, lobo_vm_evento, que
 * reinicia el script terminado pero respeta la espera. */
static int lanzar(Eventos *ev, LoboEstado *estado, int indice, int ent,
                  int repetir, const char *nota) {
    EventoScript *s = &ev->scripts[indice];
    unsigned long ins0 = 0, t0 = 0;
    if (!gastar(ev)) return 0;
    traza_evento(ev, s, indice, ent, nota);
    s->maquina.entidad = ent;
    estado->fotograma = ev->fotograma;
    if (ev->perfil_activo) {
        ins0 = lobo_perfil_instrucciones();
        t0 = perfil_ahora(ev);
        ev->perfil[indice].activo_foto = 1;
        ev->perfil[indice].disparos++;
    }
    (void)depurador_antes_bloque(s->bloque, s->evento, indice);
    if (repetir)
        lobo_vm_fotograma(&s->programa, estado, &s->maquina);
    else
        lobo_vm_evento(&s->programa, estado, &s->maquina);
    (void)depurador_despues_bloque(s->bloque, s->evento, indice,
                                   s->maquina.pc, s->maquina.agotado);
    if (ev->perfil_activo) {
        PerfilScript *p = &ev->perfil[indice];
        unsigned long t1 = perfil_ahora(ev);
        p->instrucciones += lobo_perfil_instrucciones() - ins0;
        if (t1 >= t0) p->microsegundos += t1 - t0;
    }
    if (s->maquina.detener_todo) ev->detener_todo = 1;
    if (s->maquina.cerrar) ev->cerrar = 1;
    return 1;
}

/* Continua la espera de un script de un disparo sin reiniciarlo. */
static void reanudar(Eventos *ev, LoboEstado *estado, int indice, int ent) {
    EventoScript *s = &ev->scripts[indice];
    unsigned long ins0 = 0, t0 = 0;
    s->maquina.entidad = ent;
    estado->fotograma = ev->fotograma;
    if (ev->perfil_activo) {
        ins0 = lobo_perfil_instrucciones();
        t0 = perfil_ahora(ev);
        ev->perfil[indice].activo_foto = 1;
        ev->perfil[indice].disparos++;
    }
    (void)depurador_antes_bloque(s->bloque, s->evento, indice);
    lobo_vm_reanudar(&s->programa, estado, &s->maquina);
    (void)depurador_despues_bloque(s->bloque, s->evento, indice,
                                   s->maquina.pc, s->maquina.agotado);
    if (ev->perfil_activo) {
        PerfilScript *p = &ev->perfil[indice];
        unsigned long t1 = perfil_ahora(ev);
        p->instrucciones += lobo_perfil_instrucciones() - ins0;
        if (t1 >= t0) p->microsegundos += t1 - t0;
    }
    if (s->maquina.detener_todo) ev->detener_todo = 1;
    if (s->maquina.cerrar) ev->cerrar = 1;
}

/* Un script puede recibir un disparo si no esta esperando. */
static int listo_para_evento(const EventoScript *s) {
    return s->maquina.estado != LOBO_VM_ESPERANDO;
}

/* El mensaje va dirigido al objeto del script: sin destino es broadcast;
 * con destino se compara con el tag del objeto. */
static int mensaje_para(const EventoScript *s, const EventoMensaje *m) {
    int tipo;
    if (!texto_igual(s->nombre, m->texto)) return 0;
    if (!m->destino[0]) return 1;
    tipo = entidades_tipo_por_objeto(s->objeto);
    if (tipo < 0) return 0;
    return texto_igual(entidades_tipo_nombre(tipo), m->destino);
}

/* Saca un mensaje de la cola. */
static void cola_quitar(Eventos *ev) {
    if (ev->cola_num <= 0) return;
    ev->cola_ini = (ev->cola_ini + 1) % EVENTOS_COLA_MAX;
    ev->cola_num--;
}

void eventos_fotograma(Eventos *ev, LoboEstado *estado, int fotograma,
                       unsigned entrada, unsigned anterior) {
    int i;
    if (!ev || !estado) return;

    ev->fotograma = fotograma;
    ev->eventos_fotograma = 0;
    ev->avisado = 0;
    if (ev->perfil_activo)
        for (i = 0; i < ev->num_scripts; i++)
            ev->perfil[i].activo_foto = 0;
    estado->fotograma = fotograma;
    estado->entrada = entrada;

    /* Escena detenida por `detener todo`: no se despacha nada mas. */
    if (ev->detener_todo || depurador_esta_pausado()) goto fin;

    /* 1. Eventos de sistema: inicio y escena la primera vez (o al
     * reiniciar). Se disparan una sola vez por entrada en escena. */
    if (!ev->iniciado) {
        ev->iniciado = 1;
        for (i = 0; i < ev->num_scripts; i++) {
            EventoScript *s = &ev->scripts[i];
            int ent;
            if (s->tipo != EVENTO_INICIO && s->tipo != EVENTO_ESCENA)
                continue;
            if (s->tipo == EVENTO_ESCENA && s->escena != ev->escena)
                continue;
            ent = entidad_de_objeto(s->objeto, ev->escena);
            if (ent <= 0) continue;
            if (!lanzar(ev, estado, i, ent, 0, "")) goto fin;
            if (ev->detener_todo || depurador_esta_pausado()) break;
        }
        if (ev->detener_todo || depurador_esta_pausado()) goto fin;
    }

    /* Las esperas de los scripts de un disparo siguen contando aunque no
     * llegue el evento (fotograma y tecla mantenida se descuentan en su
     * propia fase para no hacerlo dos veces). */
    for (i = 0; i < ev->num_scripts; i++) {
        EventoScript *s = &ev->scripts[i];
        int ent;
        if (s->tipo == EVENTO_FOTOGRAMA) continue;
        if (s->tipo == EVENTO_TECLA && s->flanco == 0) continue;
        if (s->maquina.estado != LOBO_VM_ESPERANDO) continue;
        ent = entidad_de_objeto(s->objeto, ev->escena);
        if (ent <= 0) continue;
        reanudar(ev, estado, i, ent);
        if (ev->detener_todo || depurador_esta_pausado()) goto fin;
    }

    /* 3. fotograma de las entidades activas, en orden de capa e id (los
     * scripts ya vienen ordenados por el lector del LOGIC3). */
    for (i = 0; i < ev->num_scripts; i++) {
        EventoScript *s = &ev->scripts[i];
        int ent;
        if (s->tipo != EVENTO_FOTOGRAMA) continue;
        ent = entidad_de_objeto(s->objeto, ev->escena);
        if (ent <= 0) continue;
        if (!lanzar(ev, estado, i, ent, 1, "")) goto fin;
        if (ev->detener_todo || depurador_esta_pausado()) goto fin;
    }

    /* 4. tecla mantenida y flancos de pulsar/soltar. */
    for (i = 0; i < ev->num_scripts; i++) {
        EventoScript *s = &ev->scripts[i];
        unsigned bit;
        int ent;
        char nota[EVENTOS_TEXTO_MAX + 16];
        if (s->tipo != EVENTO_TECLA) continue;
        bit = eventos_entrada(ev, s->nombre);
        if (!bit) continue;
        ent = entidad_de_objeto(s->objeto, ev->escena);
        if (ent <= 0) continue;

        if (s->flanco == 0) {
            if (entrada & bit) {
                _snprintf(nota, sizeof(nota), "\"%s\" mantener", s->nombre);
                nota[sizeof(nota) - 1] = '\0';
                if (!lanzar(ev, estado, i, ent, 1, nota)) goto fin;
            } else if (s->maquina.estado == LOBO_VM_ESPERANDO) {
                reanudar(ev, estado, i, ent);
            }
        } else if (s->flanco == 1) {
            if ((entrada & bit) && !(anterior & bit)) {
                if (!listo_para_evento(s)) continue;
                _snprintf(nota, sizeof(nota), "\"%s\" pulsar", s->nombre);
                nota[sizeof(nota) - 1] = '\0';
                if (!lanzar(ev, estado, i, ent, 0, nota)) goto fin;
            }
        } else {
            if (!(entrada & bit) && (anterior & bit)) {
                if (!listo_para_evento(s)) continue;
                _snprintf(nota, sizeof(nota), "\"%s\" soltar", s->nombre);
                nota[sizeof(nota) - 1] = '\0';
                if (!lanzar(ev, estado, i, ent, 0, nota)) goto fin;
            }
        }
        if (ev->detener_todo || depurador_esta_pausado()) goto fin;
    }

    /* 5. cada N: el contador empieza en N y dispara en N, 2N, 3N... */
    for (i = 0; i < ev->num_scripts; i++) {
        EventoScript *s = &ev->scripts[i];
        int ent;
        char nota[32];
        if (s->tipo != EVENTO_CADA || s->cada <= 0) continue;
        ent = entidad_de_objeto(s->objeto, ev->escena);
        if (ent <= 0) continue;
        if (s->contador > 0) s->contador--;
        if (s->contador > 0) continue;
        s->contador = s->cada;
        if (!listo_para_evento(s)) continue;
        _snprintf(nota, sizeof(nota), "%d", s->cada);
        nota[sizeof(nota) - 1] = '\0';
        if (!lanzar(ev, estado, i, ent, 0, nota)) return;
        if (ev->detener_todo || depurador_esta_pausado()) goto fin;
    }

    /* 6. tocar: colision AABB con cooldown para no repetir cada fotograma. */
    for (i = 0; i < ev->num_scripts; i++) {
        EventoScript *s = &ev->scripts[i];
        Entidad *a;
        int ent, otro;
        char nota[32];
        if (s->tipo != EVENTO_TOCAR) continue;
        ent = entidad_de_objeto(s->objeto, ev->escena);
        if (ent <= 0) continue;
        if (s->cooldown > 0) {
            s->cooldown--;
            continue;
        }
        a = entidad_buscar(ent);
        otro = entidad_tocada(s, a, ev->escena);
        if (!otro) continue;
        if (!listo_para_evento(s)) continue;
        _snprintf(nota, sizeof(nota), "%d", otro);
        nota[sizeof(nota) - 1] = '\0';
        if (!lanzar(ev, estado, i, ent, 0, nota)) return;
        s->cooldown = EVENTOS_TOCAR_ESPERA;
        if (ev->detener_todo || depurador_esta_pausado()) goto fin;
    }

    /* 7. recibir de la cola. Si el unico receptor esta esperando, el
     * mensaje se queda al frente para el fotograma siguiente. Los
     * mensajes sin receptor se descartan (broadcast normal). */
    while (ev->cola_num > 0) {
        EventoMensaje *m = &ev->cola[ev->cola_ini];
        int hay_receptor = 0, hay_listo = 0, sin_presupuesto = 0;
        for (i = 0; i < ev->num_scripts; i++) {
            EventoScript *s = &ev->scripts[i];
            int ent;
            char nota[EVENTOS_TEXTO_MAX + 8];
            if (s->tipo != EVENTO_RECIBIR) continue;
            if (!mensaje_para(s, m)) continue;
            ent = entidad_de_objeto(s->objeto, ev->escena);
            if (ent <= 0) continue;
            hay_receptor = 1;
            if (!listo_para_evento(s)) continue;
            hay_listo = 1;
            _snprintf(nota, sizeof(nota), "\"%s\"", m->texto);
            nota[sizeof(nota) - 1] = '\0';
            if (!lanzar(ev, estado, i, ent, 0, nota)) {
                sin_presupuesto = 1;
                break;
            }
            if (ev->detener_todo || depurador_esta_pausado()) goto fin;
        }
        if (sin_presupuesto) goto fin;      /* se pospone al siguiente */
        if (hay_receptor && !hay_listo) goto fin; /* espera al receptor */
        cola_quitar(ev);                  /* entregado o sin receptor */
    }

    /* 8. clonar: cada entidad creada en este fotograma dispara el evento
     * clonar de su objeto, ya con su propio id en `mi`. */
    for (i = 0; i < ENTIDADES_MAX; i++) {
        Entidad *e = entidad_indice(i);
        int objeto, j;
        if (!e || !e->usada || !e->nueva) continue;
        if (e->escena != ev->escena) continue;
        objeto = entidades_tipo_objeto(e->tipo);
        for (j = 0; j < ev->num_scripts; j++) {
            EventoScript *s = &ev->scripts[j];
            char nota[32];
            if (s->tipo != EVENTO_CLONAR || s->objeto != objeto) continue;
            if (!listo_para_evento(s)) continue;
            _snprintf(nota, sizeof(nota), "%d", e->id);
            nota[sizeof(nota) - 1] = '\0';
            if (!lanzar(ev, estado, j, e->id, 0, nota)) goto fin;
            if (ev->detener_todo || depurador_esta_pausado()) goto fin;
        }
    }

    /* 9. contador del fotograma para la traza. Las bajas y las marcas de
     * entidad nueva las limpia juego.c con entidades_fin_fotograma(). La
     * linea sale siempre (aqui o al saltar a fin), tambien cuando el
     * presupuesto corta el fotograma o un mensaje queda en espera: es la
     * prueba de que la cola no se pierde. */
fin:
    if (ev->traza && ev->detalle)
        fprintf(ev->traza, "eventos %d total %d cola %d\n", ev->fotograma,
                ev->eventos_fotograma, ev->cola_num);
}

/* ------------------------------------------------------------------ */
/* Accesores                                                           */
/* ------------------------------------------------------------------ */

int eventos_num(const Eventos *ev) {
    return ev ? ev->num_scripts : 0;
}

const EventoScript *eventos_script(const Eventos *ev, int i) {
    if (!ev || i < 0 || i >= ev->num_scripts) return NULL;
    return &ev->scripts[i];
}

int eventos_avisos(const Eventos *ev) {
    return ev ? ev->avisos : 0;
}

int eventos_ejecutados(const Eventos *ev) {
    return ev ? ev->ejecutados : 0;
}

int eventos_cerrar(const Eventos *ev) {
    return ev ? ev->cerrar : 0;
}

void eventos_perfil_activar(Eventos *ev, EventosReloj reloj, void *ctx) {
    if (!ev) return;
    memset(ev->perfil, 0, sizeof(ev->perfil));
    ev->perfil_activo = 1;
    ev->reloj = reloj;
    ev->reloj_ctx = ctx;
}

int eventos_perfil_activo(const Eventos *ev) {
    return ev ? ev->perfil_activo : 0;
}

const PerfilScript *eventos_perfil_script(const Eventos *ev, int i) {
    if (!ev || !ev->perfil_activo || i < 0 || i >= ev->num_scripts)
        return NULL;
    return &ev->perfil[i];
}

void eventos_perfil_blits(Eventos *ev, int blits) {
    int i;
    if (!ev || !ev->perfil_activo || blits <= 0) return;
    for (i = 0; i < ev->num_scripts; i++)
        if (ev->perfil[i].activo_foto)
            ev->perfil[i].blits += (unsigned long)blits;
}
