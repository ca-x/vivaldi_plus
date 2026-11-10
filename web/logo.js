(function () {
  'use strict';
  const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');

  document.querySelectorAll('.brand-logo').forEach(mark => {
    const control = mark.closest('a, button');
    const image = mark.querySelector('.logo-image');
    if (!control || !image || typeof image.animate !== 'function') return;
    let animation;

    function resetTilt() {
      image.style.transform = '';
    }

    function stop() {
      animation?.cancel();
      animation = null;
      resetTilt();
    }

    function play(pressed) {
      stop();
      if (reducedMotion.matches) return;
      animation = image.animate(pressed ? [
        { transform: 'scale(1)' },
        { transform: 'scale(.92)', offset: 0.25 },
        { transform: 'scale(1.05)', offset: 0.65 },
        { transform: 'scale(1)' }
      ] : [
        { transform: 'rotate(0deg)' },
        { transform: 'rotate(-4deg)', offset: 0.35 },
        { transform: 'rotate(3deg)', offset: 0.65 },
        { transform: 'rotate(0deg)' }
      ], { duration: 320, easing: 'ease-out' });
    }

    control.addEventListener('pointermove', event => {
      if (reducedMotion.matches || event.pointerType !== 'mouse') return;
      const bounds = mark.getBoundingClientRect();
      const x = Math.max(-1, Math.min(1, (event.clientX - bounds.left) / bounds.width * 2 - 1));
      const y = Math.max(-1, Math.min(1, (event.clientY - bounds.top) / bounds.height * 2 - 1));
      image.style.transform = `perspective(200px) rotateX(${-y * 4}deg) rotateY(${x * 4}deg)`;
    });
    control.addEventListener('pointerleave', resetTilt);
    control.addEventListener('blur', resetTilt);
    control.addEventListener('focus', () => play(false));
    control.addEventListener('click', () => play(true));
    reducedMotion.addEventListener('change', stop);
  });
})();
