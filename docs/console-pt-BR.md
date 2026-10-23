# Console para o Mi 8: teclado, terminais e configurações

Esta alteração oferece um ambiente de texto no framebuffer: teclado virtual
completo, janelas/painéis de terminal com tmux, histórico, área de transferência,
bateria, rede, horário e menu de configurações. Não instala compositor,
Xorg, Wayland nem desktop. O console continua dependendo dos drivers de display
do kernel; isto não é uma correção comprovada para o congelamento completo do
aparelho investigado anteriormente.

## Teclado não aparece no login: correção v0.1.1 por USB

A imagem `console-v0.1.0` tem um erro na consulta `KDFONTOP`: o programa
solicita dimensões máximas de fonte zeradas. O kernel retorna `ENOSPC` mesmo
sem solicitar os dados dos glifos, e o programa fica ativo sem desenhar o
teclado. O log pode mostrar somente `Touchscreen: stmfts; rotation 0`.
A correção informa capacidades válidas, usa as dimensões em pixels da tty
como alternativa e mantém o teclado visível se não puder reservar linhas.
Os testes reproduzem a falha anterior e verificam também reinício e fallback.

Para corrigir a instalação existente, baixe o ZIP de correção do release
`console-v0.1.1` no fork `otaviomorais/debian-dipper`. No Windows, extraia tudo,
conecte o Mi 8 pelo USB com o Debian na tela de login e execute
`corrigir-teclado.bat`. O pacote contém ADB, o binário ARM64 e o instalador;
não exige Wi-Fi, compilador no aparelho ou flash. Guarda o teclado anterior
em `/var/backups/dipper-keyboard.XXXXXXXX/fbkeyboard` e gera dois arquivos de
diagnóstico ao lado do `.bat`. Se o serviço não iniciar, restaura o binário
anterior. A confirmação de que o teclado aparece depende do teste no aparelho.

Com ADB já instalado, também é possível enviar a pasta `payload` do ZIP
para o aparelho e executar `apply-keyboard-fix.sh` como root. Não executar
`flash.bat`, `flash.sh` ou `fastboot erase` para aplicar esta correção.

## Gerar uma imagem que já vem pronta

O fluxo principal agora pode incorporar tudo durante a construção. Não há
instalador de primeira inicialização: dependências, teclado ARM64, configuração,
serviço habilitado e início do tmux no login já ficam gravados no rootfs.
O login normal do usuário é preservado; não é criado autologin.

No GitHub, a ação manual **Build factory console image** em **Actions** usa os
fontes deste checkout, baixa o release original `v1.0` de
`2114460639/debian-dipper`, confere SHA-256 e gera o artefato
`dipper-factory-console`. O workflow precisa estar na sua branch/repositório
para aparecer nessa lista. Não é executado automaticamente nem publica um
release. As imagens antigas do upstream permanecem com o código antigo.

Para construir em um computador Linux, instalar as ferramentas do host:

```bash
sudo apt-get update
sudo apt-get install e2fsprogs android-sdk-libsparse-utils python3 make
# Em x86_64, também é necessário executar binários ARM64 no chroot:
sudo apt-get install qemu-user-static binfmt-support
sudo bash scripts/enable-arm64-binfmt.sh
```

Extrair `images/boot.img` e `images/xiaomi-dipper.img` do pacote original
`debian-dipper-flash-console.7z`, depois de verificar o SHA-256 publicado no
release. Usar os dois arquivos do mesmo pacote, de uma versão compatível com
o Xiaomi Mi 8. A imagem base pode ser Android sparse ou raw ext4.

```bash
sudo make image \
  BASE_ROOTFS=/caminho/images/xiaomi-dipper.img \
  BOOT_IMAGE=/caminho/images/boot.img \
  OUTPUT_DIR=/caminho/saida-nova
```

O diretório de saída deve ser novo. O construtor trabalha numa cópia, verifica
o filesystem, instala os pacotes Debian com verificação normal de assinaturas,
compila e testa dentro do rootfs ARM64 e incorpora os arquivos do console.
Remove apenas pacotes adicionados exclusivamente para compilar; se a remoção
afetaria pacotes anteriores ou de execução, a construção é interrompida.
Não inicia serviços no chroot. DNS, política temporária de serviços e CA do
host usados na construção são restaurados antes de empacotar.

Ao final, reduz a imagem e gera Android sparse RAW+FILL com verificação de ida
e volta. O limite padrão é 2150 MiB de cobertura, seguindo o tamanho usado pelo
projeto. Se o mínimo estimado pelo resize2fs ultrapassar o limite, reconstrói
um ext4 compacto com o UUID, rótulo e recursos originais, verificando conteúdo,
proprietários, permissões, hardlinks, links simbólicos e atributos estendidos
de todos os arquivos antes de aceitar o resultado. Se não couber ou a verificação
falhar, a construção é interrompida. Não aumentar o limite sem conferir a
compatibilidade do bootloader.

A saída contém `images/boot.img`, `images/xiaomi-dipper.img`, SHA-256, MD5,
`flash.sh`, README e uma cópia raw para inspeção. O kernel/boot vêm da base
compatível: esta construção não recompila nem corrige o kernel. A imagem mantém
o UUID e rótulo do ext4, para continuar compatível com o boot fornecido.
O script não acessa o aparelho. O `flash.sh` só é usado depois, em outra etapa;
ele apaga userdata, como no procedimento original.

O fluxo antigo de montagem do rootfs também pode usar a integração diretamente:

```bash
# Primeiro, instalar os pacotes de config/console-packages.list no rootfs.
# Compilar com a toolchain/dependências ARM64 selecionadas, não a do PC:
make rootfs ROOTFS_DIR=/mnt/rootfs FBKEYBOARD=/caminho/fbkeyboard-arm64
```

`make rootfs` recusa um binário de arquitetura incorreta. Instala somente os
arquivos do console, escolhe `multi-user.target` como padrão e habilita o teclado.
Não inicia processos do host e não substitui as outras adaptações de hardware
já existentes na imagem base.

Essa integração foi testada com um rootfs temporário e o binário real do host:
arquivos, permissões, habilitação, início no login, arquitetura e repetibilidade.
Também foi construída uma imagem usando o release original com SHA-256
verificado: compilação e testes ARM64 via QEMU, verificação de 34.544 entradas
após compactação, e2fsck e conversão sparse com comparação de ida e volta.
O boot foi preservado byte a byte; o rootfs final cobre 2150 MiB.
Na imagem resultante, fbkeyboard, Python/status, tmux e dialog executaram via
QEMU; o serviço aparece habilitado e o target padrão é multi-user.
O boot físico e o toque ainda precisam ser testados no Mi 8. O workflow
disponibilizado ainda não foi executado no GitHub.

## Atualizar um aparelho já instalado

Não é preciso reinstalar ou apagar o Debian para aplicar esta alteração.
Use este checkout atualizado ou o pacote de fontes entregue junto com a mudança;
um clone do upstream anterior não contém os arquivos novos.

No Mi 8, dentro do diretório dos fontes:

```bash
sudo bash scripts/install-console.sh --autostart
dipper-console
```

O instalador exige aarch64 e acesso root. Compila o teclado no próprio aparelho,
roda os testes, salva backup e substitui os programas/configurações relacionados.
Instala libevdev, FreeType, fontes DejaVu, tmux, terminfo, dialog, NetworkManager,
htop e mc pelos repositórios Debian já configurados. Não grava partições, não
muda kernel/firmware, não desativa desktops instalados e não configura login
automático. Se um desktop estiver ativo, sair dele e entrar num VT de texto
para usar o teclado. O teclado pausa a pintura e entrada em VTs gráficos.

`--autostart` inicia a sessão depois do login normal na tty1, sem afetar SSH ou
root. Sem essa opção, abra `dipper-console` quando desejar. Para desligar o
início automático: `sudo rm /etc/dipper-console/autostart`.

O script reinicia `fbkeyboard.service` ao terminar; o teclado pode desaparecer
brevemente. Instalar por SSH é conveniente. `--no-restart` deixa o serviço
parado após a substituição, para iniciá-lo manualmente. `--no-packages` só
serve quando todas as dependências já estão instaladas. Nenhuma instalação
ou reinicialização de serviço foi executada num Mi 8 durante o desenvolvimento.
O drop-in de fonte é atualizado para o caminho Debian opcional: a ausência
da antiga fonte Alpine não impede mais o serviço de iniciar.

Se uma instalação falhar depois de começar a substituir arquivos, o script tenta
restaurar o backup automaticamente. O caminho do backup também é informado
em caso de sucesso. Para voltar à versão anterior manualmente:

```bash
sudo bash scripts/restore-console.sh /var/backups/dipper-console.XXXXXXXX
```

Substitua pelo caminho real informado. A restauração repõe os arquivos antigos
e o estado ativo/inativo anterior do serviço; não remove os pacotes Debian que
tenham sido instalados. Uma sessão tmux já aberta mantém sua configuração até
ser encerrada ou recarregada. As configurações deste ambiente usam um servidor
tmux separado e não sobrescrevem `~/.tmux.conf`.

## Teclado

São sete linhas visíveis, com letras, números, símbolos de shell, setas,
Home/End, PgUp/PgDn, Insert/Delete, Escape/Tab/Enter, F1–F12 e atalhos.
`123` alterna os símbolos; `Fn` abre as funções e símbolos adicionais.
Os rótulos de pontuação pressupõem o mapa de teclado US, como na imagem original.

Shift, Ctrl e Alt selecionam um modificador para **a próxima tecla**. O estado
selecionado fica destacado. O dispositivo virtual recebe pressionar e soltar
a combinação inteira; nenhum modificador é deixado fisicamente pressionado.
Para cancelar a seleção, toque novamente no modificador ou use `Reset`.
Exemplos: Ctrl e depois C interrompe um comando; Ctrl e depois A envia o prefixo
do tmux; Shift e depois uma letra produz a maiúscula correspondente.

Um toque digita ao levantar o dedo. Arrastar para fora da tecla original cancela
o toque. Dois dedos simultâneos cancelam o gesto até ambos serem retirados;
não é um teclado multitoque com acordes físicos. É possível tocar o mesmo lugar
rapidamente várias vezes sem depender de novas coordenadas do driver.

Backspace, Delete e setas repetem após 450 ms, a cada 90 ms. Por proteção contra
um driver que nunca envie o fim do toque, o gesto é cancelado depois de 3 s;
retire e toque novamente para continuar. Perda de eventos (`SYN_DROPPED`)
ressincroniza o estado com libevdev e cancela o gesto sem inventar uma tecla.
Mudança de VT, entrada em modo gráfico e encerramento também limpam o estado.

O teclado só suporta framebuffer RGB de 32 bits e touchscreen multitouch com
slots, correspondentes ao alvo dipper. Pode selecionar o touchscreen com `-d`
e rotação com `-r 0..3`. Sem `-r`, lê a rotação do fbcon na inicialização.
Mudanças de rotação/fonte em tempo de execução exigem reiniciar o teclado.

## Terminais e configurações

| Botão | Função no Dipper Console |
| --- | --- |
| Menu / F1 | Configurações e aplicativos de texto |
| New / F2 | Novo terminal |
| Prev / F3 e Next / F4 | Janela anterior/próxima |
| Split / F5 | Dividir o terminal em dois painéis |
| Zoom / F6 | Ampliar/restaurar o painel |
| Pane / F7 | Próximo painel |
| Hist / F8 | Histórico do terminal |
| Cmd / F9 | Linha de comandos do tmux |
| Select / F10 | Iniciar seleção de texto |
| Copy / F11 | Copiar a seleção e voltar |
| Paste / F12 | Colar o último texto copiado |

Os atalhos F1–F12 são usados por este ambiente. Fora dele, os botões enviam as
teclas de função normais; eles não gerenciam outros terminais por conta própria.
Dentro dele, para enviar uma tecla de função a um aplicativo que a utilize,
abra `Cmd` e digite, por exemplo, `send-keys F1`.

A barra superior mostra bateria, indicação `+` ao carregar, interface/IP de rede
e horário local do sistema. Atualiza a cada cinco segundos. A indicação de rede
mostra o link/IPv4, não garante acesso à Internet nem testa servidores externos.
A tela usa a fonte do console configurada no aparelho.

No menu, **Rede** abre `nmtui`: conectar Wi-Fi, editar conexões, IP e DNS.
Setas, Tab e Enter permitem operar todos os diálogos. Credenciais ficam sob
controle do NetworkManager; o ambiente não as lê nem as coloca na barra de status.
A autorização depende do polkit da instalação (a imagem original já inclui
uma regra para gerenciamento de rede pelo usuário). O instalador não adiciona
regras novas de privilégio. Htop monitora processos e mc gerencia arquivos.

## Copiar e colar

1. Toque **Hist** para abrir o histórico.
2. Use setas, Home/End e PgUp/PgDn para chegar ao começo do texto.
3. Toque **Select** e use as setas para ampliar a seleção.
4. Toque **Copy**. O terminal volta ao modo normal.
5. Troque de janela/painel se desejar e toque **Paste**.

`Cancel`/Esc abandona a seleção. Ctrl+C continua sendo interrupção de comando,
e não copiar. O buffer pertence ao servidor tmux do seu usuário: funciona entre
seus painéis e janelas, mas não é integrado automaticamente ao clipboard de
Android, Windows, SSH ou de um desktop gráfico. O envio por OSC 52 está desligado.
O buffer/histórico fica na sessão, sem gravação automática em arquivo.

Colar usa o modo bracketed paste quando o aplicativo de destino o suporta;
texto com novas linhas ainda deve ser revisado antes de colar no shell. Comandos
podem ser executados ao receber uma nova linha em aplicativos sem essa proteção.

## Verificações realizadas e limites

Os testes de C exercitam o código real de envio de teclas, libevdev simulado,
toques rápidos, repetições, perda de sincronização, múltiplos dedos, troca de
tracking ID, timeout e limites de layout. As três páginas são renderizadas nas
quatro rotações, sob AddressSanitizer e UndefinedBehaviorSanitizer.

```bash
make -C fbkeyboard check
python3 scripts/test_console.py -v
```

O teste de Python verifica bateria/rede com dados locais e abre um servidor
tmux temporário real para testar copiar texto, colá-lo em outra janela, dividir
painéis e ampliar. O teste de tmux é pulado se o programa não estiver instalado.

A compilação e esses testes foram executados em x86_64 e no rootfs ARM64 via QEMU.
O teste físico de toque,
uinput, tamanho do console, Wi-Fi/polkit e estabilidade ainda depende do Mi 8.
Os testes não demonstram que um problema no driver de touchscreen ou no kernel
do aparelho tenha desaparecido. Se houver congelamento total, preserve os logs
indicados no diagnóstico anterior.
