(() => {
    const canvas = document.getElementById('ambient');
    const ctx = canvas.getContext('2d');
    const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    const isDark = window.matchMedia('(prefers-color-scheme: dark)').matches;

    function resize() {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
    }
    window.addEventListener('resize', resize);
    resize();

    function rand(min, max) { return Math.random() * (max - min) + min; }
    function randInt(min, max) { return Math.floor(rand(min, max + 1)); }

    const BRUSH_TYPES = ['radial', 'linear', 'sweep'];

    class AmbientCircle {
        constructor() { this.respawn(true); }

        respawn(randomizeProgress = false) {
            const w = Math.max(canvas.width, 1);
            const h = Math.max(canvas.height, 1);

            this.durationMs = rand(9000, 27000);
            this.elapsedMs = randomizeProgress ? Math.random() * this.durationMs : 0;
            this.radius = rand(142, 384);

            this.baseX = rand(0, w);
            this.baseY = rand(0, h);

            this.driftFromX = rand(0, 64);
            this.driftFromY = rand(0, 64);
            this.driftToX = rand(0, 64);
            this.driftToY = rand(0, 64);

            this.brushType = BRUSH_TYPES[randInt(0, BRUSH_TYPES.length - 1)];
            this.gradientSeed = {
                cx: rand(0, w), cy: rand(0, h), r: rand(0, h),
                sx: rand(0, w / 4), sy: rand(0, h / 4),
                ex: rand(0, (w * 3) / 4), ey: rand(0, (h * 3) / 4),
                angle: rand(0, Math.PI * 2),
            };
        }

        update(dtMs) {
            this.elapsedMs += dtMs;
            if (this.elapsedMs >= this.durationMs) this.respawn();
        }

        get progress() { return Math.min(Math.max(this.elapsedMs / this.durationMs, 0), 1); }
        get alpha() { return Math.max(Math.min(Math.sin(this.progress * Math.PI), 1), 0); }

        get center() {
            const p = this.progress;
            const dx = this.driftFromX + (this.driftToX - this.driftFromX) * p;
            const dy = this.driftFromY + (this.driftToY - this.driftFromY) * p;
            return { x: this.baseX + dx, y: this.baseY + dy };
        }

        gradient() {
            const s = this.gradientSeed;
            if (this.brushType === 'radial') {
                const g = ctx.createRadialGradient(s.cx, s.cy, 0, s.cx, s.cy, s.r || 1);
                g.addColorStop(0, '#00FFFF');
                g.addColorStop(0.5, '#FF00FF');
                g.addColorStop(1, '#0000FF');
                return g;
            }
            if (this.brushType === 'linear') {
                const g = ctx.createLinearGradient(s.sx, s.sy, s.ex, s.ey);
                g.addColorStop(0, '#683997');
                g.addColorStop(0.33, '#2f8ed7');
                g.addColorStop(0.66, '#2d73ff');
                g.addColorStop(1, '#3b1f64');
                return g;
            }
            const c = this.center;
            if (ctx.createConicGradient) {
                const g = ctx.createConicGradient(s.angle, c.x, c.y);
                g.addColorStop(0, '#51103e');
                g.addColorStop(0.33, '#bb244e');
                g.addColorStop(0.66, '#f9bd2f');
                g.addColorStop(1, '#df5327');
                return g;
            }
            const g = ctx.createRadialGradient(c.x, c.y, 0, c.x, c.y, this.radius);
            g.addColorStop(0, '#51103e');
            g.addColorStop(0.5, '#bb244e');
            g.addColorStop(1, '#df5327');
            return g;
        }

        draw() {
            const c = this.center;
            ctx.globalAlpha = this.alpha;
            ctx.globalCompositeOperation = isDark ? 'lighter' : 'multiply';
            ctx.fillStyle = this.gradient();
            ctx.beginPath();
            ctx.arc(c.x, c.y, this.radius, 0, Math.PI * 2);
            ctx.fill();
        }
    }

    const circles = Array.from({ length: 16 }, () => new AmbientCircle());
    let lastTime = 0;

    function frame(time) {
        const dt = lastTime ? Math.min(Math.max(time - lastTime, 0), 64) : 0;
        lastTime = time;

        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        for (const circle of circles) {
            circle.update(dt);
            circle.draw();
        }

        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;

        requestAnimationFrame(frame);
    }

    if (reduceMotion) {
        for (const circle of circles) circle.draw();
    } else {
        requestAnimationFrame(frame);
    }
})();
