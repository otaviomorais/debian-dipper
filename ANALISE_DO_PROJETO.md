# Análise do Projeto: Debian-Dipper (Xiaomi Mi 8)

Este projeto é uma **ROM customizada baseada em Debian GNU/Linux 13 (trixie) para arquitetura aarch64**, desenvolvida especificamente para o smartphone **Xiaomi Mi 8 (codinome "dipper")**, que utiliza o SoC Qualcomm Snapdragon 845.

O foco principal desta imagem não é fornecer um ambiente de desktop típico de smartphones (como Phosh ou Plasma Mobile), mas sim um **ambiente de terminal Linux puro (framebuffer console)** totalmente funcional e estável, com um teclado virtual integrado para digitação por toque na tela.

## Principais Características e Objetivos

1. **Abordagem "Console-First" (Sem Interface Gráfica):**
   - O sistema inicializa diretamente no terminal texto do Linux (`tty1`).
   - Não há gerenciadores de exibição gráfica (display managers) ou ambientes de desktop (GUI).
   - O idioma do console está configurado para inglês (`en_US.UTF-8`).
   - Ideal para quem deseja um servidor portátil, ambiente de desenvolvimento via terminal, ou uma plataforma base Linux estável no hardware do Xiaomi Mi 8.

2. **Teclado Virtual Integrado (`fbkeyboard`):**
   - Um teclado virtual de tela cheia é renderizado na metade inferior da tela.
   - Suporta navegação (teclas de seta, Home, End), rolagem de logs (Shift+PageUp/PageDown customizado no kernel para funcionar no framebuffer), e digitação usando a tela sensível ao toque.
   - Foram aplicadas correções personalizadas para evitar o travamento de teclas (problemas de "key stuck") e melhorar a resposta ao toque.

3. **Kernel Customizado (baseado no postmarketOS):**
   - Utiliza o kernel Linux versão `7.1.0-rc1-sdm845`.
   - Inclui patches específicos (`.patch`) para a tela DSI (Driver DRM), modo de escaneamento do touch screen, e funcionalidade de rolagem de tela no console framebuffer (`fbcon-scrollback.patch`).

4. **Suporte Robusto de Hardware:**
   - **GPU:** Suporte para 3D (Adreno 630) usando drivers freedreno/turnip.
   - **Conectividade:** Wi-Fi, Bluetooth e Dados Móveis 4G (Modem) estão funcionais.
   - **Áudio:** O áudio funciona através do ALSA UCM (Dipper-HiFi), com correções no ganho do microfone.
   - **USB:** Suporta USB Net (rede pelo cabo USB para SSH/ADB com IP fixo `172.16.42.1`), USB OTG (suporte para pen drives) e carregamento rápido via Power Delivery (PD).
   - **Câmera e Suspensão:** A câmera **não é suportada por design**, e o modo de suspensão/hibernação foi intencionalmente **desativado** (a tela simplesmente desliga após 10 minutos para poupar bateria sem desligar os processos).

## Estrutura do Repositório

O repositório armazena os "métodos de adaptação" (scripts, patches, configurações), enquanto as imagens binárias pesadas (`.img`) e ferramentas de terceiros são distribuídas via GitHub Releases (e intencionalmente ignoradas pelo `.gitignore`).

- `flash.sh` / `flash.bat`: Scripts de automação para facilitar o "flash" (instalação da ROM) pelo modo bootloader (fastboot) no Linux e no Windows.
- `kernel/`: Contém os patches aplicados ao kernel principal, o arquivo de configuração de compilação do postmarketOS e documentação.
- `rootfs/`: A estrutura de diretórios injetada no sistema raiz contendo arquivos de configuração específicos para o Mi 8. Isso inclui:
  - Regras do systemd (ex: para teclado, modem, USB, controle de bateria).
  - Configurações de rede, udev e sudoers.
- `scripts/`: Scripts em Python para conversão do sistema de arquivos e empacotamento da partição de boot (`repack_boot.py`, `mk_sparse_fill.py`).
- `fbkeyboard/`: Código-fonte em C (`fbkeyboard.c`) e `Makefile` do teclado virtual do framebuffer modificado para o projeto.
- `README.md`: Documentação extremamente detalhada (em chinês) que cobre desde instruções de instalação passo-a-passo, limitações, explicações técnicas dos patches e como o sistema funciona sob o capô.

## Tecnologias e Metodologias Utilizadas

- **Debootstrap:** A construção do sistema de arquivos raiz (`rootfs`) é feita baseada nos repositórios oficiais do Debian.
- **Android Sparse Images:** O sistema é flasheado na partição `userdata` usando o formato esparso do Android (em vez de arquivos raw .img), o que agiliza drasticamente a instalação pelo `fastboot`.
- **Systemd:** Altamente integrado e dependente do `systemd` para gerenciar hardware, inicializar o modem, rede e aplicar configurações (ex: `systemd-growfs-root.service` para expandir a partição na primeira inicialização).
- **Gerenciamento de Energia:** Possui um serviço próprio (`polaris-power-guard.service`) que implementa um desligamento limpo quando a bateria está crítica, protegendo o armazenamento flash UFS de corrupção.

## Resumo e Conclusão

Este projeto demonstra um alto nível de engenharia e conhecimento profundo do ecossistema de kernel Linux para dispositivos móveis (soc QCOM/Snapdragon). Em vez de focar na usabilidade gráfica comum para smartphones (onde a aceleração e interface podem ser pesadas e instáveis), o autor criou uma plataforma Linux "headless" robusta, focada no acesso direto ao terminal e no uso como micro-servidor ou estação de linha de comando portátil.
