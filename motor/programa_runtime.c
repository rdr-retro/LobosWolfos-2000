/* programa_runtime.c - runtime del constructor visual (primera entrega E9). */
#include <string.h>
#include "programa.h"
#include "entidades.h"
#include "colisiones.h"

static int texto_igual(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static int es_teclado(const MotorL3Control *c) {
    return !c || !c->dispositivo[0] || texto_igual(c->dispositivo, "teclado");
}

static int slot_de_entrada(const char *nombre) {
    int slot;
    if (!nombre || !nombre[0]) return -1;
    slot = motor_entrada_slot(nombre);
    if (slot >= 0) return slot;
    if (texto_igual(nombre, "left")) return ENT_IZQUIERDA;
    if (texto_igual(nombre, "right")) return ENT_DERECHA;
    if (texto_igual(nombre, "up")) return ENT_ARRIBA;
    if (texto_igual(nombre, "down")) return ENT_ABAJO;
    if (texto_igual(nombre, "jump")) return ENT_ESPACIO;
    return -1;
}

static int entrada_activa(const char *nombre, int flanco,
                          const MotorEntrada *ext, unsigned clasica) {
    int slot = slot_de_entrada(nombre);
    int valor;
    if (slot < 0) return 0;
    if (!ext) {
        if (flanco != 0) return 0;
        switch (slot) {
        case ENT_IZQUIERDA: return (clasica & MOTOR_ENT_IZQUIERDA) != 0;
        case ENT_DERECHA:   return (clasica & MOTOR_ENT_DERECHA) != 0;
        case ENT_ARRIBA:    return (clasica & MOTOR_ENT_ARRIBA) != 0;
        case ENT_ABAJO:     return (clasica & MOTOR_ENT_ABAJO) != 0;
        case ENT_ESPACIO:   return (clasica & MOTOR_ENT_SALTO) != 0;
        default: return 0;
        }
    }
    if (flanco == 1) valor = motor_entrada_pulsada(ext, slot);
    else if (flanco == 2) valor = motor_entrada_soltada(ext, slot);
    else valor = motor_entrada_mantenida(ext, slot);
    return valor != 0;
}

static int funcion_direccion(const char *funcion, const char *entrada) {
    const char *s = funcion && funcion[0] ? funcion : entrada;
    if (!s) return 0;
    if (strstr(s, "izquierda") || strstr(s, "left")) return -1;
    if (strstr(s, "derecha") || strstr(s, "right")) return 1;
    if (strstr(s, "arriba") || strstr(s, "up")) return -2;
    if (strstr(s, "abajo") || strstr(s, "down")) return 2;
    return 0;
}

const MotorL3Control *motor_programa_control_principal(const MotorL3 *p) {
    int i;
    if (!p) return NULL;
    for (i = 0; i < p->sistema.num_controles; i++)
        if (p->sistema.controles[i].activo)
            return &p->sistema.controles[i];
    return NULL;
}

int motor_programa_movimiento(const MotorL3Control *control,
                              const MotorEntrada *entrada_ext,
                              unsigned clasica, int velocidad,
                              int *dx, int *dy) {
    int i, izq = 0, der = 0, arriba = 0, abajo = 0;
    int tiene_acciones = 0;
    if (dx) *dx = 0;
    if (dy) *dy = 0;
    if (!control || !control->activo || !es_teclado(control)) return 0;
    if (velocidad < 1) velocidad = 1;

    for (i = 0; i < control->num_acciones; i++) {
        const MotorL3Accion *a = &control->acciones[i];
        int dir;
        if (!a->entrada[0]) continue;
        tiene_acciones = 1;
        if (!entrada_activa(a->entrada, a->flanco, entrada_ext, clasica))
            continue;
        dir = funcion_direccion(a->funcion, a->entrada);
        if (dir == -1) izq = 1;
        else if (dir == 1) der = 1;
        else if (dir == -2) arriba = 1;
        else if (dir == 2) abajo = 1;
    }

    /* Un control sin acciones usa el esquema basico de plataformas. Esto
     * permite que Actor + Teclado sea jugable desde la primera entrega. */
    if (!tiene_acciones) {
        izq = entrada_activa("izquierda", 0, entrada_ext, clasica);
        der = entrada_activa("derecha", 0, entrada_ext, clasica);
        arriba = entrada_activa("arriba", 0, entrada_ext, clasica);
        abajo = entrada_activa("abajo", 0, entrada_ext, clasica);
    }
    if (dx) *dx = (der - izq) * velocidad;
    if (dy) *dy = (abajo - arriba) * velocidad;
    return (izq || der || arriba || abajo) ? 1 : 0;
}

int motor_programa_aplicar(const MotorL3 *p, const MotorEntrada *entrada_ext,
                           unsigned clasica, const MotorMapa *mapa,
                           int velocidad) {
    int i, ids[ENTIDADES_MAX], total = 0;
    if (!p) return 0;
    for (i = 0; i < p->sistema.num_controles; i++) {
        const MotorL3Control *c = &p->sistema.controles[i];
        int n, dx, dy, id, x, y, w, h;
        if (!c->activo || c->actor <= 0) continue;
        if (!motor_programa_movimiento(c, entrada_ext, clasica, velocidad,
                                       &dx, &dy) || (!dx && !dy)) continue;
        n = entidades_de_objeto(c->actor, ids, ENTIDADES_MAX);
        if (n < 1) continue;
        id = ids[0];
        x = entidades_leer_campo(id, ENTIDAD_CAMPO_X);
        y = entidades_leer_campo(id, ENTIDAD_CAMPO_Y);
        w = entidades_leer_campo(id, ENTIDAD_CAMPO_W);
        h = entidades_leer_campo(id, ENTIDAD_CAMPO_H);
        if (w < 1) w = 16;
        if (h < 1) h = 16;
        if (mapa) {
            int triggers[8], nt = 0, dano = 0;
            colisionar_caja_mapa(mapa, &x, &y, w, h, dx, dy,
                                 triggers, 8, &nt, &dano);
        } else {
            x += dx;
            y += dy;
        }
        if (entidad_mover(id, x, y) > 0) total++;
    }
    return total;
}
