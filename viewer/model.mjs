export function clampExplosion(value) {
  const number = Number(value);
  return Number.isFinite(number) ? Math.max(0, Math.min(1, number)) : 0;
}

export function partPosition(part, amount) {
  const factor = clampExplosion(amount);
  return factor === 0 ? [0, 0, 0] : part.explode.map(value => value * factor);
}

export function partVisible(part, visibility, isolated, hidden) {
  return (!isolated || part.id === isolated) && !hidden.has(part.id) &&
    (part.category === 'printed' || Boolean(visibility[part.category]));
}
