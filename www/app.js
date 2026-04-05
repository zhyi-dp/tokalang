document.addEventListener('DOMContentLoaded', () => {

    // --- Intersection Observer for Scroll Reveals ---
    const observerOptions = {
        threshold: 0.1,
        rootMargin: '0px 0px -50px 0px'
    };

    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.classList.add('revealed');
                // Optional: Stop observing once revealed
                // observer.unobserve(entry.target);
            }
        });
    }, observerOptions);

    const revealElements = document.querySelectorAll('[data-reveal]');
    revealElements.forEach((el, index) => {
        // Add a small stagger effect based on index for elements in the same viewport
        el.style.transitionDelay = `${index * 0.05}s`;
        observer.observe(el);
    });

    // --- Smooth Scrolling for Anchor Links ---
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function(e) {
            e.preventDefault();
            const targetId = this.getAttribute('href');
            if (targetId === '#') return;

            const targetElement = document.querySelector(targetId);
            if (targetElement) {
                window.scrollTo({
                    top: targetElement.offsetTop - 80, // Offset for fixed nav
                    behavior: 'smooth'
                });
            }
        });
    });

    // --- Code Copy (Optional) ---
    const codeBlock = document.querySelector('.code-content');
    if (codeBlock) {
        codeBlock.addEventListener('click', () => {
            const text = codeBlock.innerText;
            navigator.clipboard.writeText(text).then(() => {
                // Subtle feedback
                console.log('Code copied to clipboard');
                // You could add a toast here if needed
            });
        });
    }

    // --- Navbar Background on Scroll ---
    const nav = document.querySelector('nav');
    window.addEventListener('scroll', () => {
        if (window.scrollY > 20) {
            nav.style.background = 'rgba(10, 11, 16, 0.9)';
            nav.style.boxShadow = '0 10px 30px rgba(0,0,0,0.3)';
        } else {
            nav.style.background = 'rgba(10, 11, 16, 0.6)';
            nav.style.boxShadow = 'none';
        }
    });

});
