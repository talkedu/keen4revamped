// Brazilian Portuguese.
//
// Exe strings are named by their address (see engine/lang.h); the English
// original is at that address in the player's own KEEN4E.EXE and is not
// copied here. Windows in the game are sized for the English
// text, so translated lines are kept about as wide, and the help articles
// keep the original's picture commands and page breaks.
#include "engine/lang.h"

namespace keen4::lang {

namespace {

constexpr ExeEntry kExe[] = {
    // Level names (CK_GAME's far table at DGROUP 18F4)
    {FAR(0x1F04, 0), "Terras Sombrias"},
    {FAR(0x1F05, 0), "Vila da Fronteira"},
    {FAR(0x1F06, 0), "Vila das Lesmas"},
    {FAR(0x1F07, 0), "O Poço Perigoso"},
    {FAR(0x1F09, 0), "Caverna dos\nDescendentes"},
    {FAR(0x1F0B, 0), "Abismo dos Arrepios"},
    {FAR(0x1F0C, 0), "Crystalus"},
    {FAR(0x1F0D, 0), "Hillville"},
    {FAR(0x1F0E, 0), "Sand Yego"},
    {FAR(0x1F0F, 0), "Miragia"},
    {FAR(0x1F10, 0), "Oásis da Água da Vida"},
    {FAR(0x1F11, 0), "Pirâmide das Luas"},
    {FAR(0x1F13, 0), "Pirâmide das Sombras"},
    {FAR(0x1F15, 0), "Pirâmide dos Anciões\nGnosticenos"},
    {FAR(0x1F18, 0), "Pirâmide do Proibido"},
    {FAR(0x1F1A, 0), "Ilha do Piche"},
    {FAR(0x1F1B, 0), "Ilha do Fogo"},
    {FAR(0x1F1C, 0), "Poço dos Desejos"},
    {FAR(0x1F1D, 0), "Megafoguete\nFeijão-com-Bacon"},

    // Entering a level (DGROUP 1958)
    {FAR(0x1F1F, 0), "Keen entra nas\nTerras Sombrias"},
    {FAR(0x1F21, 0), "Keen corre para a\nVila da Fronteira"},
    {FAR(0x1F24, 0), "Keen se esgueira na\nVila das Lesmas"},
    {FAR(0x1F26, 0), "Keen despenca no\nPoço Perigoso"},
    {FAR(0x1F29, 0), "Keen desce com calma\naté a Caverna dos\nDescendentes"},
    {FAR(0x1F2D, 0), "Keen treme pelo\nAbismo dos Arrepios"},
    {FAR(0x1F30, 0), "Keen reflete antes\nde entrar em Crystalus"},
    {FAR(0x1F33, 0), "Keen tropeça em\nHillville"},
    {FAR(0x1F35, 0), "Keen range os dentes\ne entra em Sand Yego"},
    {FAR(0x1F38, 0), "Keen some em\nMiragia"},
    {FAR(0x1F3A, 0), "Keen se arrasta até o\nOásis da Água da Vida"},
    {FAR(0x1F3D, 0), "Keen entra de costas\nna Pirâmide das Luas"},
    {FAR(0x1F40, 0), "Keen se move calado\nna Pirâmide das Sombras"},
    {FAR(0x1F43, 0), "Keen entra com fervor\nna Pirâmide dos\nAnciões Gnosticenos"},
    {FAR(0x1F47, 0), "Keen hesita, mas entra\nna Pirâmide do\nProibido"},
    {FAR(0x1F4B, 0), "Keen chafurda pela\nIlha do Piche"},
    {FAR(0x1F4E, 0), "Keen dispara pela\nIlha do Fogo"},
    {FAR(0x1F51, 0), "Keen entra esperançoso\nno Poço dos Desejos"},
    {FAR(0x1F54, 0), "Keen embarca no\nMegafoguete\nFeijão-com-Bacon"},

    // The janitor in the Pyramid of the Forbidden
    {FAR(0x1F58, 0), "Obrigado por todo esse\ntrabalho, mas eu sou\nsó o zelador do\nAlto Conselho."},
    {FAR(0x1F5E, 0), "Eu tentei explicar\nisso aos Shikadi, mas\neles não me ouviram..."},
    {FAR(0x1F63, 0), "É bom que isso\nseja uma piada."},
    {FAR(0x1F65, 0), "Desculpe.  Você não\nficou bravo, ficou?"},

    // CK_GAME, CK_PLAY and CK_TEXT
    {DG(0x024E), "Fim de Jogo!"},
    {DG(0x0259), "Memória insuficiente\npara carregar o jogo!"},
    {DG(0x0279), "Memória insuficiente\npara carregar o jogo!"},
    {DG(0x029E), "  Gravar demo da fase (0-21):"},
    {DG(0x02C0), "  Salvar como demo (0-9):"},
    {DG(0x02FF), "Você não passou de"},
    {DG(0x0317), "Tentar de Novo"},
    {DG(0x0321), "Voltar ao Mapa"},
    {DG(0x0335), "Memória insuficiente\npara carregar a fase!"},
    {DG(0x0389), "Um momento"},
    {DG(0x0764), "pág. "},
    {DG(0x0768), " de "},

    // K4_SPEC: Princess Lindsey, the wetsuit and the council members
    {DG(0x1ADC), "Há um equipamento para\nnadar no Lago Três-\nDentes. Ele está\nescondido em Miragia.\n"},
    {DG(0x1B29), "O caminho para a\nPirâmide do Proibido\nfica embaixo da\nPirâmide das Luas.\n"},
    {DG(0x1B73), "Obrigado, Alteza!"},
    {DG(0x1B8A), "Obrigado pela pista\nmisteriosa,\nPrincesa!\n"},
    {DG(0x1BB5), "A Princesa Lindsey diz:\n"},
    {DG(0x1BCD), "Eu não sei nadar!"},
    {DG(0x1BDB), "Legal!  Agora eu\nrespiro debaixo\nd'água!"},
    {DG(0x1C01), "Moleza, ó guardião\nda sabedoria!"},
    {DG(0x1C22), "Parece um bom plano,\nbarbudo!"},
    {DG(0x1C43), "Sem problemas."},
    {DG(0x1C50), "Ótimo.  Sabe, você é\na cara do último\nque eu resgatei..."},
    {DG(0x1C90), "Boa ideia, vovô."},
    {DG(0x1CA3), "Que a estrada se\nabra aos seus pés,\nSr. Conselheiro."},
    {DG(0x1CD4), "Sábio plano de ação,\nvossa antiguidade."},
    {DG(0x1CFB), "Você é o último,\ncamarada.  Vamos\nvoltar à câmara\ndo Oráculo!"},
    {DG(0x1D43), "Mgguito ogbrigado\npogr me regsgatar!\nVogltarei pgara a\ncâmagra do Ográculo\nimegdiatagmente. Blub."},
    {DG(0x1DA7), "Muito obrigado\npor me resgatar!\nVoltarei para a\ncâmara do Oráculo\nimediatamente."},

    // ID_CA, ID_IN key names, ID_US
    {DG(0x3BB9), "Carregando"},
    {DG(0x3E50), "Espaço"},
    {DG(0x3EA1), "Início"},
    {DG(0x3EAB), "Fim"},
    {DG(0x3EC2), "Cima"},
    {DG(0x3EC5), "Baixo"},
    {DG(0x3ECA), "Esq."},
    {DG(0x3ECF), "Dir."},
    {DG(0x46B2), "Vazio"},

    // The control panel (ID_US_2): menu items
    {DG(0x4782), "SEM EFEITOS SONOROS"},
    {DG(0x4793), "ALTO-FALANTE DO PC"},
    {DG(0x47B1), "ADLIB/SOUNDBLASTER BAIXO"},
    {DG(0x47CA), "SEM MÚSICA"},
    {DG(0x47E6), "COMEÇAR NO FÁCIL"},
    {DG(0x47F6), "COMEÇAR NO NORMAL"},
    {DG(0x4808), "COMEÇAR NO DIFÍCIL"},
    {DG(0x481C), "CIMA+ESQUERDA"},
    {DG(0x4826), "CIMA"},
    {DG(0x4829), "CIMA+DIREITA"},
    {DG(0x4834), "DIREITA"},
    {DG(0x483A), "BAIXO+DIREITA"},
    {DG(0x4847), "BAIXO"},
    {DG(0x484C), "BAIXO+ESQUERDA"},
    {DG(0x4858), "ESQUERDA"},
    {DG(0x485D), "PULAR"},
    {DG(0x4867), "ATIRAR"},
    {DG(0x486C), "MOVIMENTO"},
    {DG(0x4875), "BOTÕES"},
    {DG(0x487D), "SOM"},
    {DG(0x4883), "MÚSICA"},
    {DG(0x4889), "OPÇÕES"},
    {DG(0x4891), "USAR TECLADO"},
    {DG(0x489E), "USAR JOYSTICK 1"},
    {DG(0x48AE), "USAR JOYSTICK 2"},
    {DG(0x48BF), "NOVO JOGO"},
    {DG(0x48C8), "CARREGAR JOGO"},
    {DG(0x48D2), "SALVAR JOGO"},
    {DG(0x48DC), "CONFIGURAR"},
    {DG(0x48E6), "ENCERRAR JOGO"},
    {DG(0x48EF), "GUERRA DE RAQUETES"},
    {DG(0x48FA), "SAIR"},
    {DG(0x4E5A), "VOLTAR AO JOGO"},
    {DG(0x4E69), "VOLTAR À DEMO"},

    // The control panel: dialogs and footers
    {DG(0x4922), "ESC PARA VOLTAR"},
    {DG(0x4932), "ENCERRAR O JOGO ATUAL?"},
    {DG(0x494B), "APERTE Y PARA ENCERRAR"},
    {DG(0x495D), "SAIR MESMO?"},
    {DG(0x496A), "APERTE Y PARA SAIR"},
    {DG(0x497A), "HÁ UM JOGO EM ANDAMENTO"},
    {DG(0x498B), "APERTE Y PARA CARREGAR"},
    {DG(0x49A0), "HÁ UM JOGO EM ANDAMENTO"},
    {DG(0x49B1), "APERTE Y PARA NOVO JOGO"},
    {DG(0x49C6), "Erro: "},
    {DG(0x49CE), "desconhecido"},
    {DG(0x49D6), "disco cheio"},
    {DG(0x49E3), "arquivo incompleto"},
    {DG(0x49F6), "APERTE UMA TECLA"},
    {DG(0x4A04), "PLACAR (SIM)"},
    {DG(0x4A13), "PLACAR (NÃO)"},
    {DG(0x4A23), "TIRO COM 2 BOTÕES (SIM)"},
    {DG(0x4A3A), "TIRO COM 2 BOTÕES (NÃO)"},
    {DG(0x4A52), "ANTI-TRANCOS (SIM)"},
    {DG(0x4A68), "ANTI-TRANCOS (NÃO)"},
    {DG(0x4A7F), "COMPATÍVEL C/ SVGA (SIM)"},
    {DG(0x4A97), "COMPATÍVEL C/ SVGA (NÃO)"},
    {DG(0x4AB0), "GRAVIS GAMEPAD (SIM)"},
    {DG(0x4AC8), "GRAVIS GAMEPAD (NÃO)"},
    {DG(0x4AE1), "Placar ligado"},
    {DG(0x4AF2), "Placar desligado"},
    {DG(0x4B04), "Aperte uma tecla"},
    {DG(0x4B12), "Anti-trancos ligado"},
    {DG(0x4B2B), "Anti-trancos desligado"},
    {DG(0x4B45), "Aperte uma tecla"},
    {DG(0x4B53), "Modo compatível ligado"},
    {DG(0x4B6D), "Modo compatível desligado"},
    {DG(0x4B88), "Aperte uma tecla"},
    {DG(0x4B96), "Tiro com dois botões ligado"},
    {DG(0x4BAF), "Tiro com dois botões desligado"},
    {DG(0x4BC9), "Aperte uma tecla"},
    {DG(0x4BD7), "TECLADO"},
    {DG(0x4BE0), "TECLADO"},
    {DG(0x4BE9), "JOYSTICK 1"},
    {DG(0x4BF5), "JOYSTICK 2"},
    {DG(0x4C07), "CONTROLE: "},
    {DG(0x4C11), "Tecla já usada"},
    {DG(0x4C22), "Aperte uma tecla"},
    {DG(0x4C31), "ESC para voltar"},
    {DG(0x4C41), "Leve o joystick ao alto e"},
    {DG(0x4C5D), "à esquerda e aperte o 1"},
    {DG(0x4C71), "Leve o joystick abaixo e"},
    {DG(0x4C8E), "à direita e aperte o 2"},
    {DG(0x4CA2), "USANDO JOYSTICK 1"},
    {DG(0x4CB4), "APERTE UMA TECLA"},
    {DG(0x4CC2), "USANDO JOYSTICK 2"},
    {DG(0x4CD4), "APERTE UMA TECLA"},
    {DG(0x4CE2), "Verm."},
    {DG(0x4CE6), "Azul"},
    {DG(0x4CEB), "Amarelo"},
    {DG(0x4CF2), "Verde"},
    {DG(0x4CF8), "Pular"},
    {DG(0x4D02), "Atirar"},
    {DG(0x4D0E), "Confira se a chave dos"},
    {DG(0x4D28), "botões está em 4 botões"},
    {DG(0x4D48), "APERTE O BOTÃO: "},
    {DG(0x4D5B), "ESC para voltar"},
    {DG(0x4D6B), "PULAR"},
    {DG(0x4D75), "ATIRAR"},
    {DG(0x4D81), "APERTE UMA TECLA"},
    {DG(0x4D90), "Carregando"},
    {DG(0x4D9C), "Vazio"},
    {DG(0x4DA2), "Digite o nome"},
    {DG(0x4DAC), "Enter confirma"},
    {DG(0x4DBA), "Sem nome"},
    {DG(0x4DC3), "Salvando"},
    {DG(0x4DD4), "PC:"},
    {DG(0x4DDA), "Você perdeu!"},
    {DG(0x4DE4), "Aperte uma tecla"},
    {DG(0x4DF2), "Você venceu!"},
    {DG(0x4DFB), "Aperte uma tecla"},
    {DG(0x4E09), "Setas movem"},
    {DG(0x4E15), "Enter escolhe"},
    {DG(0x4E23), "ESC para voltar"},
    {DG(0x4E33), "ESC para sair"},
    {DG(0x4E78), "Painel de Controle"},
    {DG(0x4EA1), "Saindo..."},
};

// Text the port prints from its own source (CK_PLAY, co-op).
constexpr TextEntry kText[] = {
    {"God mode OFF", "Modo Deus OFF"},
    {"God mode ON", "Modo Deus ON"},
    {"Free items!", "Itens grátis!"},
    {"Jump cheat ON", "Pulo infinito ON"},
    {"Jump cheat OFF", "Pulo infinito OFF"},
    {"No clipping ON", "Sem colisão ON"},
    {"No clipping OFF", "Sem colisão OFF"},
    {"Slow motion ON", "Câmera lenta ON"},
    {"Slow motion OFF", "Câmera lenta OFF"},
    {"  Add how many extra VBLs(0-8):", "  Quantos VBLs a mais (0-8):"},
    {"  Warp to which level(1-18):", "  Ir para qual fase (1-18):"},
    {" Border color (0-15):", " Cor da borda (0-15):"},
    {"Cheat Option!\n\nYou just got all\nthe keys, 99 shots,\nand an extra keen!",
     "Trapaça!\n\nVocê ganhou todas\nas chaves, 99 tiros\ne um Keen extra!"},
    {"PAUSED", "PAUSADO"},
    {"LOCATION", "LOCAL"},
    {"SCORE", "PONTOS"},
    {"EXTRA", "EXTRA"},
    {"RESCUED", "SALVOS"},
    {"LEVEL", "NÍVEL"},
    {"Easy", "Fácil"},
    {"Hard", "Difícil"},
    {"KEYS", "JOIAS"},  // the keys are gems, and CHAVES runs into the box
    {"AMMO", "TIROS"},
    {"KEENS", "VIDAS"},
    {"DROPS", "GOTAS"},
    {"Wetsuit", "Traje"},
    {"Debug keys active", "Teclas de teste ativas"},
    {"Memory Usage", "Uso de Memória"},
    // co-op (ID_US_2 main menu and its device setup)
    {"CO-OP GAME", "JOGO COOPERATIVO"},
    {"CONNECT A GAMEPAD", "CONECTE UM CONTROLE"},
    {"PLAYER 1: BILLY", "JOGADOR 1: BILLY"},
    {"PRESS A ON A GAMEPAD", "APERTE A NUM CONTROLE"},
    {"OR ANY KEY", "OU QUALQUER TECLA"},
    {"PLAYER 2: TOM", "JOGADOR 2: TOM"},
    {"PRESS A ON ANOTHER GAMEPAD", "APERTE A NOUTRO CONTROLE"},
    // co-op in play (CK_COOP)
    {"WAITING", "ESPERANDO"},
    {"NO LIVES", "SEM VIDAS"},
    {"Reconnect gamepad", "Reconecte o controle"},
    // the VIDEO, CONTROLS and LANGUAGE cards (ID_US_2 configure card)
    {"VIDEO", "VÍDEO"},
    {"CONTROLS", "CONTROLES"},
    {"LANGUAGE", "IDIOMA"},
    {"SCREEN", "TELA"},
    {"FULL SCREEN", "TELA CHEIA"},
    {"WINDOW", "JANELA"},
    {"REFRESH RATE", "FREQUÊNCIA"},
    {"VSYNC", "VSYNC"},
    {"ON", "LIGADO"},
    {"OFF", "DESLIGADO"},
    {"GRAPHICS", "GRÁFICOS"},
    {"UPSCALED", "MELHORADOS"},
    {"ORIGINAL", "ORIGINAIS"},
    {"RESOLUTION", "RESOLUÇÃO"},
    {"AUTO", "AUTOMÁTICA"},
    // the updater (ID_US_2 main menu)
    {"CHECK FOR UPDATES", "ATUALIZAR O JOGO"},
    {"CONNECTING TO GITHUB", "CONECTANDO AO GITHUB"},
    {"SIGN IN TO GITHUB", "ENTRE NO GITHUB"},
    {"OPEN", "ABRA"},
    {"AND ENTER THIS CODE:", "E DIGITE ESTE CÓDIGO:"},
    {"SIGN-IN CANCELED", "LOGIN CANCELADO"},
    {"ON GITHUB", "NO GITHUB"},
    {"THE CODE EXPIRED", "O CÓDIGO EXPIROU"},
    {"PLEASE TRY AGAIN", "TENTE DE NOVO"},
    {"UPDATES ARE OFF", "ATUALIZAÇÕES DESLIGADAS"},
    {"IN THIS BUILD", "NESTA VERSÃO"},
    {"NO ACCESS TO THE GAME", "SEM ACESSO AO JOGO"},
    {"ON THIS GITHUB ACCOUNT", "NESTA CONTA DO GITHUB"},
    {"INSTALL THE GITHUB APP", "INSTALE O APP DO GITHUB"},
    {"ON THE GAME'S REPOSITORY", "NO REPOSITÓRIO DO JOGO"},
    {"NO RELEASES YET", "AINDA NÃO HÁ VERSÕES"},
    {"NO UPDATE AVAILABLE", "NENHUMA ATUALIZAÇÃO"},
    {"IS THE LATEST", "É A MAIS NOVA"},
    {"NEW VERSION:", "NOVA VERSÃO:"},
    {"PRESS Y TO UPDATE", "APERTE Y PARA ATUALIZAR"},
    {"DOWNLOADING", "BAIXANDO"},
    {"INSTALLING", "INSTALANDO"},
    {"PLEASE WAIT", "AGUARDE"},
    {"UPDATE FAILED", "FALHA AO ATUALIZAR"},
    {"UPDATE CANCELED", "ATUALIZAÇÃO CANCELADA"},
    {"WINDOWS DID NOT ALLOW IT", "O WINDOWS NÃO PERMITIU"},
    {"UPDATED TO", "ATUALIZADO PARA"},
    {"TO RESTART THE GAME", "PARA REINICIAR O JOGO"},
};

// ComputerWrist help (chunk 4739)
constexpr char kHelpComputerWrist[] = R"(^P
^G87,8,7
^G112,8,9
^G140,8,8
^ceVocê está na Ajuda do ComputerWrist, o computador de pulso do Keen. Ele está aqui para ajudar você a jogar Commander Keen do jeito que preferir.  Primeiro, veja como ler este artigo de ajuda.



Use as setas para avançar e voltar as páginas da Ajuda. Quando terminar de ler, aperte Esc.


Aperte a seta para a direita agora para continuar.
^P
^G120,8,10
^cFMENU PRINCIPAL DO COMPUTERWRIST
^ce
O Menu Principal tem nove opções: Novo Jogo, Carregar Jogo, Salvar Jogo, Configurar, Voltar à Demo, Encerrar Jogo, Guerra de Raquetes, Idioma e Sair.  A opção escolhida fica destacada, com um ponto piscando ao lado.  As opções indisponíveis têm um "X".


Use as setas para ir até a opção desejada e aperte Enter para escolhê-la.
^P
^G50,8,27
Vamos ver cada seção em ordem.

^cbNOVO JOGO
^ce
Você pode começar um jogo novo no modo fácil, normal ou difícil. O modo define quantas criaturas estarão contra você e quantos tiros você ganha a cada Atordoador Neural.

Se estiver no meio de um jogo que não foi salvo, começar outro apaga o jogo antigo para sempre. O jogo pede confirmação antes.
^P
^cbCARREGAR JOGO
^ce
Se você salvou um jogo antes, esta opção carrega o jogo para você continuar.  Se ainda não salvou nenhum (como quando acaba de ganhar o jogo), a opção não fica disponível.

^cbSALVAR JOGO
^ce
Você pode salvar até seis jogos diferentes, mas só depois de começar um, claro.  Dê o nome que quiser.  Vá até um espaço livre com as setas, aperte Enter, digite o nome e aperte Enter para salvar.  Depois aperte ESC para voltar ao Menu Principal.
^P
^cbMENU CONFIGURAR
^ce
Este menu tem as opções de som, de controle e alguns ajustes do jogo.

Para os Efeitos Sonoros, as escolhas são: sem efeitos, efeitos no alto-falante do PC, efeitos na Adlib/Sound Blaster ou efeitos baixos na Adlib/Sound Blaster.

Para a Música, as escolhas são: nenhuma ou Adlib/Sound Blaster.
^P
^cbOPÇÕES
^ce
Você escolhe se quer ver o placar no canto da tela enquanto joga.  É bom tê-lo ali, mas às vezes ele atrapalha a visão!  Em computadores lentos, tirar o placar é uma ótima ideia.  Basta apertar Enter durante o jogo para abrir a Janela de Status.
^P
^cbOPÇÕES (continuação)
^ce
A segunda opção é o Tiro com 2 Botões.  Se você joga com joystick, atirar pelo teclado pode ser chato.  Com esta opção, o Atordoador dispara quando os dois botões estão apertados.  Os pulos altos de pula-pula ficam mais difíceis, mas você se acostuma.

A terceira opção é "Anti-Trancos".  Se a tela der muitos trancos ao rolar, ligue esta opção. Veja a seção "Parâmetros de Linha de Comando" para outro jeito de fazer isso.
^P
^cbOPÇÕES (continuação)
^ce
A quarta opção do menu Opções é a Compatibilidade com SVGA.  Se você tem uma placa SVGA e a imagem dá problema, ligue este modo para a placa funcionar com o Keen.

Você também pode jogar com teclado ou joystick.  Com o teclado, dá para mudar as teclas de controle.  Com o joystick um ou dois, siga as instruções de calibragem.  O controle em uso aparece embaixo da janela do menu.
^P
^cBUSAR GRAVIS GAMEPAD
^cE
O Gravis Gamepad é um controle de quatro botões para PC.  Ele funciona como os controles dos videogames famosos.

Para usá-lo, escolha "Usar Joystick 1" ou "Usar Joystick 2", conforme a entrada onde ele está ligado.  Aí você poderá escolher "Gravis Gamepad" e definir a função de cada botão.  (Aliás, na época dava para encomendar o Gamepad da Apogee pelo 1-800-GAME123.)
^P
^cbVOLTAR À DEMO/AO JOGO
^ce
Se você abriu o ComputerWrist durante um jogo, esta opção volta para a ação.  Se não, ela volta para a demonstração.

^cbENCERRAR JOGO
^ce
Se quiser encerrar o jogo atual por qualquer motivo, use esta opção.  A demonstração volta a passar.
^P
^cbGUERRA DE RAQUETES
^ce
Enquanto montava seu ComputerWrist, Commander Keen resolveu colocar nele um joguinho simples de raquetes.  Ele terminou a "Guerra de Raquetes" bem na hora em que ouviu a mensagem dos Shikadi.  Outros jogos devem vir depois.

^cbIDIOMA
^ce
Troca o idioma dos textos do jogo entre inglês e português.

^cbSAIR
^ce
Sair, na falta de palavra melhor, sai.
^P
^cfAVISO AOS DONOS DE SUPER VGA
^ce
Se você tem uma placa Speedstar VGA ou outra placa SVGA não 100% compatível, digite "KEEN4 /COMP" na linha de comando, em vez de só "KEEN4". Isso deve resolver todos os problemas.  Outro jeito é escolher Configurar, depois Opções e depois Compatível c/ SVGA no menu do ComputerWrist.

^P
^cfOUTROS PARÂMETROS BACANAS DE LINHA DE COMANDO
^ce
Quando um manual manda digitar algo além do nome do programa (como fizemos acima), esses pedaços extras se chamam "parâmetros de linha de comando".  É um nome chique para as informações extras que você passa ao programa ao abri-lo. Normalmente você nunca vai precisar deles.  Se o programa tiver problemas com o seu equipamento, tente o parâmetro adequado.
^P
Alguns parâmetros de linha de comando que funcionam com Commander Keen:

  KEEN4 /NOAL  (Não detecta AdLib nem Sound Blaster.)
  KEEN4 /NOJOYS  (Ignora o joystick.)
  KEEN4 /NOMOUSE  (Ignora o mouse.)
  KEEN4 /HIDDENCARD  (Ignora a detecção da placa de vídeo, se o programa estiver detectando a placa errado e não deixar você jogar.)
  KEEN4 /JERK  (Se a tela der muitos trancos ao rolar para os lados, isto deve resolver.)
^P
^G100,108,43
^CfCONCLUSÃO
^ce
Agora vá explorar o ComputerWrist e divirta-se jogando "O Segredo do Oráculo"!
^E)";

// Game play (chunk 4741)
constexpr char kHelpGamePlay[] = R"(^p
^cFAS TECLAS DA EMOÇÃO
^cE
Vamos ver todas as ações do jogo (as teclas citadas são as do jogo novo, sem mudanças).  Para saber como configurar o teclado ou o joystick, aperte Esc e escolha "Ajuda com o ComputerWrist" no menu de ajuda. Commander Keen pode:

^cBCORRER PARA OS LADOS
^cEUse as setas para a esquerda e para a direita para mover o Keen.

^cBPEGAR COISAS
^cEPara pegar algo, basta passar por cima.
^P
^cBPULAR
^cEAperte Ctrl para o Keen pular. (É o Botão 1 no joystick.)

^cBPULA-PULA
^cEAperte Alt para usar o pula-pula.  Com ele você pula mais alto, mas é mais difícil de controlar. Aperte Alt de novo para guardá-lo. (É o Botão 2 no joystick.)

^cBATIRAR
^cEAperte a barra de espaço (ou Ctrl e Alt juntos, se escolheu Tiro com 2 Botões) para disparar o Atordoador Neural. O tiro sai na direção em que você está virado.
^P
^cBENTRAR
^cESe estiver na frente de uma porta, aperte para cima para entrar.

^cBSUBIR NO MASTRO
^cEAperte para cima ou para baixo na frente de um mastro para se agarrar nele. Aperte para cima ou para baixo para subir ou escorregar.  Apertando para a esquerda ou a direita, você gira para aquele lado. Para sair rápido do mastro, aperte Pular (Ctrl).
^P
^cBAGARRAR NA BEIRADA
^cESe errar o alvo do pulo, você pode se agarrar na beirada.  Aperte para cima ou na direção da plataforma (esquerda ou direita) para subir. Se apertar para baixo ou para o lado contrário, você cai.

^cBACIONAR INTERRUPTOR
^cEViu um interruptor (com uma luz vermelha ou verde)? Pare na frente dele e aperte para cima.  Interruptores mudam coisas ao redor, como, digamos, pontes, plataformas e afins.
^P
^cBDESCER
^cESe estiver numa plataforma ou num piso estreito, aperte a seta para baixo e pule para descer.

^cBOLHAR PARA CIMA/BAIXO
^cEAperte a seta para cima ou para baixo para ver mais da fase naquela direção.  Isso ajuda MUITO quando parece não haver onde pular.  Olhando para cima ou para baixo, você pode achar coisas escondidas.

^cBABRIR PORTAS
^cEAs portas precisam de chaves em forma de joia encaixadas no suporte. Para encaixar a joia, pegue-a e vá até o suporte.
^P
Para terminar uma fase, você precisa:
 a) sair pela placa de saída (Exit),
 ou b) achar o Conselheiro da fase.

O jogo tem montes de itens bônus e salas secretas escondidas.  Explore!
^P
^cFMOVIMENTOS AVANÇADOS
^cE
Alguns movimentos levam você aos lugares "impossíveis de alcançar".  Aqui estão eles:

^cBPULO IMPOSSÍVEL
^cECorra para um lado e aperte Pula-pula e Pular rapidinho, um depois do outro: você decola em diagonal. Com o Tiro com 2 Botões, este movimento fica bem difícil.

^cBPULA-PULA NO AR
^cEVocê pode sacar o pula-pula no meio de um pulo normal, o que dá bom controle desse primeiro pulo.
^P
^cBATIRAR PARA BAIXO
^cEVocê pode pular sobre um monstro e atirar nele de cima, apertando a seta para baixo e atirando.  Também dá para atirar para cima ou atirar pendurado num mastro.

^cBSUBIDA RÁPIDA NO MASTRO
^cEPara subir mais rápido, segure a seta para cima e aperte Pular várias vezes.  Exige prática, mas vale a pena.
^P
^cBPULOS DESCOLADOS
^cEEm muitos lugares há um grande arco de doces.  Em geral dá para pegar todos em um só pulo descolado.

Existem outros movimentos difíceis, mas deixamos você descobri-los sozinho.
^P
^G30,8,20
^G50,8,21
^G70,8,22
^G90,8,23
^G110,8,24
^G130,8,25
^cFITENS QUE O KEEN PODE PEGAR
^cE
Refrigerante Shikadi (100 pontos)

Chiclete Três-Dentes (200 pontos)

Chocolate Shikkers (500 pontos)

Quebra-Queixo (1000 pontos)

Rosquinha (2000 pontos)

Casquinha de Sorvete (5000 pontos)
^P
^G85,8,27
^G105,8,28
^G125,8,26
Gotas de chuva flutuam no céu.  Como a água de Gnosticus IV tem energia vital, se o Keen juntar cem gotas, ganha uma vida extra!

E há alguns itens úteis também.


Atordoador Neural (mais tiros)

Joias (abrem portas)

Frasco de Água da Vida (um Keen extra)
^p
^G66,8,29
^G116,8,30
^cFELENCO DE PERSONAGENS
^cE
Há muitos inimigos nas Terras Sombrias. Esta é uma lista dos mais comuns:

^cbARACNOZ. ^ce Criaturas verdes e malucas que andam de lado, com duas bocas perigosas.


^cbBERKELOIDE. ^ce Com este sujeito flamejante não se brinca.  Ele joga grandes bolas de fogo!

^P
^G10,8,31
^G70,8,32
^cbQUICADOR. ^ce Estes caras vermelhos e simpáticos vivem quicando. Às vezes irritam, mas também podem ajudar!



^cbCONSELHEIRO. ^ceSão os Gnosticenos que você quer resgatar.  Há oito deles escondidos nas profundezas das Terras Sombrias.
^P
^G10,8,33
^G130,8,34
^cbPEIXE-BOBO. ^ce A segunda criatura mais tonta do universo. Seus pensamentos são "nada nada fome, nada nada fome".  Come qualquer coisa viva que se mexa por perto, mas prefere heróis.




^cbMINHOCA. ^ce Estas criaturas amarelas e calmas estão por toda parte.  Cuidado onde pisa!
^P
^G10,8,35
^G60,8,36
^cbLAMBIDA. ^ce Estes monstros azuis com cara de pufe soltam uma lambida mortal de fogo.


^cbCOGUMELO LOUCO. ^ce Plantas perigosas que saltam e matam ao toque.
^P
^G10,8,37
^G80,8,38
^cbLESMA VENENOSA. ^ce O habitante mais comum das Terras Sombrias. É encrenca dupla: uma vez se você tocar nela, outra se cair nas poças de veneno que ela deixa para trás!

^cbPRINCESA LINDSEY. ^ce A Princesa está aqui para ajudar.  Se você a encontrar, ela dará dicas valiosas para completar sua missão.
^P
^G10,8,39
^G50,8,40
^G110,8,41
^cbCARDUME. ^ce Estes peixes adoram seguir o líder.


^cbPRAGA-DO-CÉU. ^ce Quase imunes aos seus tiros, estes caras são uma praga mesmo.  Mas deve haver algum jeito de pegá-los.


^cbDUENDE. ^ce Nem o Peixe-Bobo mexe com estas criaturas aquáticas encrenqueiras: elas disparam rajadas de energia em você!
^P
^G10,8,42
^cbBOCA-DE-VERME. ^ce Cuidado com esta ameaça verde.  Ela é bem maior do que parece!



Há mais monstros por aí, mas descobri-los sozinho é metade da graça!
^P
^cFPROBLEMAS?
^cE
Se, ao abrir Commander Keen, aparecer a janela "Out of Memory" (sem memória), há algumas saídas.

^cbTIRE OS TSRS. ^ceSe o seu AUTOEXEC.BAT não carregar programas residentes (TSRs), eles não ocupam a memória de que o Keen precisa.  Recomendamos renomear o AUTOEXEC.BAT para AUTOEXEC.OLD e editar uma cópia dele.
^P
^cbCARREGUE O DOS NA MEMÓRIA ALTA. ^ceCom pelo menos um 286, alguma memória estendida e o DOS 5.0, você pode carregar o DOS na memória alta. Basta pôr "DOS=HIGH" no arquivo CONFIG.SYS.

^cbOUTRAS IDEIAS. ^ce O Keen usa blocos de memória superior e EMS quando existem.  (Se você tem uma placa EMS ou um computador 386, consulte o manual do sistema.)

^P
^G100,108,43
Isso é tudo!  Agora vá jogar Commander Keen!
^E)";

// The story (chunk 4740)
constexpr char kHelpStory[] = R"(^P
^cFCOMMANDER KEEN em . . . "Adeus, Galáxia!"
Keen Episódio Quatro: O Segredo do Oráculo
^cE
Billy Blaze trabalha na oficina do quintal, ocupado com sua mais nova invenção: o Transceptor Fotáquion.  Em palavras simples, é um rádio instantâneo: aponte para qualquer lugar da galáxia e capte o sinal como se estivesse lá.  Depois de captar um monte de seriados alienígenas muito ruins, Billy ouve uma mensagem estranha, na língua Omnifala que os Vorticons lhe ensinaram....
^P
^G11,8,12
    brrzzz....giggg....omento de grande triunfo...frzzt....a Via Láctea será... huzzzzz...talmente destruída...pyuneeeeeeg...pode nos deter.  Vamos refazer a galáxia em nome do Gannalech.  Poder à nossa raça!  Poder aos Shikadi!

Então o rádio fica mudo.

"Então os Shikadi querem destruir a Galáxia, é?" pensa Billy.  "Parece trabalho para...COMMANDER KEEN!"
^P
Billy veste mais uma vez o capacete de futebol americano do irmão, pensando em três perguntas:
^cB
     Quem são os tais Shikadi?
     Onde eles estão?
     Como pretendem destruir a galáxia?
^cE
Ao entrar na cabine do Megafoguete Feijão-com-Bacon, Billy programa a decolagem para Gnosticus IV, lar dos Gnosticenos, guardiões do Oráculo.  Eles lhe devem um favor, e ele precisa de respostas.  A contagem começa....
^P
^G38,8,13
^G115,8,15
"Hora da janta, Billy!" anuncia a mãe, entrando na oficina.

"Ah, droga!" solta Billy.  Ele percebe que não dá tempo de parar a contagem e esconder a nave!  Pensando rápido, saca o Atordoador Neural, mira na porta e espera.


Um "Zzzzap!" depois, a mãe está congelada na porta.
^P
^G58,8,14
^G110,108,15
"Ufa", diz Keen.

"Querida?  O que houve?" pergunta o pai de Billy, saindo pela porta dos fundos.

"Droga em dobro!" resmunga Billy.  Ele espia por trás da mãe, mira e....
^P
^G30,8,18
^G50,260,16
^G90,8,18
Em Gnosticus IV, um Pajem do Conselho corre até ele.

"Oh, Capitão Keen!" começa ele.

"É Commander Keen", corrige Billy.

"Os Shikadi estiveram aqui, sequestraram os membros do Alto Conselho e os levaram para as Terras Sombrias, bem a oeste."
^P
^G10,8,16
^G30,260,18
^G90,8,16
"Eles não os mataram?" pergunta Keen.

"Impossível.  Eles são imortais", diz o pajem.  "Então só garantiram que eles não pudessem ativar o Oráculo."

"Bem, então eu vou buscá-los", diz Keen, virando-se com determinação heroica.
^P
^G10,8,18
^G50,260,16
"Hã, mas Capitão Keen, eles também deixaram um monte de criaturas horríveis e armadilhas de guarda", diz o Pajem do Conselho.

"Então vou buscá-los COM CUIDADO", diz Keen por cima do ombro.

Keen vai até a porta, para, vira-se e fala com o pajem.
^P
^G10,260,17
^G100,16,19
^G140,108,43
"E se me chamar de Capitão Keen de novo, vou levar você junto."

Keen sai decidido enquanto o pajem murmura: "Desculpe."

Keen pula no Megafoguete Feijão-com-Bacon e parte para as Terras Sombrias....
^E)";

// How to order (chunk 4745). The offers are from 1992 and kept as history.
constexpr char kHelpOrder[] = R"(^P
^cFCOMO ENCOMENDAR COMMANDER KEEN
^ce
Você está jogando o episódio um de Commander Keen: "Adeus, Galáxia!", chamado "O Segredo do Oráculo".  É uma aventura épica em duas partes, então não perca o emocionante episódio final, "A Máquina do Armagedom".

Para saber como e onde encomendar as próximas aventuras de Commander Keen, leia as telas a seguir.

CLIENTES DE FORA DOS EUA: leiam "Pedidos pelo Mundo" para os detalhes.
^P
^G8,8,86
^P
^CFCOMO ENCOMENDAR "ADEUS, GALÁXIA!"
^cE
Nos Estados Unidos, peça direto à Apogee Software.  Para a saga "Adeus, Galáxia!" completa, envie US$ 39 (com US$ 4 de frete etc.) para:

         Apogee Software
         P.O. Box 476389
         Garland, TX   75047

Importante:  informe o tamanho do disquete e se você tem VGA, EGA ou CGA.

Ou ligue de graça no horário comercial:
^cB1-800-GAME123  (1-800-426-3123)
^P
^G86,240,46
^G126,240,44
^cFENCOMENDANDO "ADEUS, GALÁXIA!" (cont.)
^cE
Atenção:  "Adeus, Galáxia!" tem dois episódios, e nenhum deles pode ser registrado sozinho.  Ao encomendar "Adeus, Galáxia!" para ter o segundo episódio ("A Máquina do Armagedom"), você também registra este, "O Segredo do Oráculo".

Todos os pedidos incluem:
* A folha de Dicas e Truques
* Códigos secretos de trapaça!
* Um novo jogo bônus

Ligue ^cB1-800-GAME123 ^cEpara pedir no horário comercial.  Aceitamos Visa e MasterCard!
^P
^cFENCOMENDE O EPISÓDIO DOIS DE "ADEUS, GALÁXIA!"
^cE
O episódio dois, "A Máquina do Armagedom", é uma aventura bem maior que "O Segredo do Oráculo".  Bons motivos para encomendá-lo:

* 13 músicas novas para AdLib/Sound Blaster
* Mais de 20 criaturas novas
* Dezenas de áreas secretas
* Veja a cara de um Shikadi
* Mais de 1,6 MEGABYTES de gráficos novos
* Salve a Galáxia de explodir
* Diversão garantida!
^P
^cFEPISÓDIO DOIS (cont.)
^cE
Lembre-se: ao encomendar o episódio dois de "Adeus, Galáxia!", você recebe uma cópia nova e atualizada do episódio um e fica registrado na série toda.

Aí você pode ajudar Commander Keen a salvar a Galáxia!
^P
^cFOUTRAS OFERTAS SÓ DA APOGEE
^cE
Agora existem seis episódios de Commander Keen!  São eles:
^cBCommander Keen: "Invasion of the Vorticons"
^cEUm conjunto de três episódios:
1) Marooned on Mars
2) The Earth Explodes
3) Keen Must Die!

^cBCommander Keen: "Goodbye, Galaxy!"
^cEUm conjunto de dois episódios:
1) Secret of the Oracle
2) The Armageddon Machine

^cBCommander Keen: "Aliens Ate My Baby Sitter!"
^cEUm episódio "Edição Especial" (veja a pág. 9).
^P
^cFPREÇOS E DESCONTOS DA APOGEE
^cE
Preços, só nos EUA, dos seis episódios de Commander Keen, com ótimas ofertas de desconto:

"Invasion of the Vorticons" - US$ 30
"Goodbye, Galaxy!" - US$ 35
"Aliens Ate My Baby Sitter!" - US$ 35

^cBDesconto especial:
^cE"Goodbye, Galaxy!" E
"Aliens Ate My Baby Sitter!" - US$ 65
^P
^cFPREÇOS E DESCONTOS DA APOGEE
^cE
^cBSuper Pacote Keen:
^cEOs seis episódios - US$ 90
(Só US$ 15 por jogo!)

Ligue de graça nos EUA e no Canadá:
^cB1-800-GAME123
^cE
Acrescente sempre US$ 4 de frete.
^P
^cF"ALIENS ATE MY BABY SITTER!"
^cE
É um episódio "Edição Especial" que se passa depois de "Adeus, Galáxia!"  (Se o Keen tiver sucesso!)

Billy precisa salvar Molly, sua babá, sequestrada por alienígenas para o jantar, e ela é o prato principal!  Parece trabalho para . . . Commander Keen!

Mais de ^cB2 MEGAS ENORMES ^cEde gráficos, trilha completa para AdLib e muita ação espacial hilária.  Não há versão shareware, mas a demo de três fases sai por só US$ 3.  Ou encomende o jogo completo pelo preço da pág. 7.
^P
^G8,8,87
"Baby Sitter!" é diversão de montão!
^P
^cFOUTROS BÔNUS ETC.
^cE
Em qualquer pedido, a Apogee envia:

* A folha de Dicas e Truques
* Senha secreta de trapaça
* Novo jogo bônus da Apogee

Ao pedir pelo correio, informe o tamanho do disquete, 3,5" ou 5,25", e se você tem CGA, EGA ou VGA.

Ao pedir pelo telefone gratuito da Apogee, ^cB1-800-426-3123, ^cEligue no horário comercial:  seg. a sex., das 8h às 19h (horário central dos EUA).
^P
^cFSOFTWARE CREATIONS BBS
^cE
A BBS oficial da Apogee Software
"Dedicated to the Development
 and Distribution of Shareware"

Para baixar on-line os jogos mais novos da Apogee:
^cB(508)365-2359 ^cElinhas de 2400/9600 baud

^P
A Software Creations BBS tem, a qualquer hora, os novos sharewares da Apogee. Conecte-se e confira a seção de arquivos "Apogee".

^cFSÓ PARA SYSOPS!
^cE
Fale com Dan Linton (sysop) para entrar na Rede Oficial de Distribuição de BBSs da Apogee e receber as novidades, as descrições certas dos arquivos etc. Deixe um C)omentário ao sysop.

^P
^cFPEDIDOS PELO MUNDO
^cE
^cBAUSTRÁLIA
^cEUS$ 45 + US$ 5 de frete  (US$ 50 no total)

Manaccom
9 Camford Str.
Milton, QLD 4064
Australia

Telefone:  (07) 368-2366

Aceita:  Mastercard, Visa, Bankcard, Amex.
^P
^cBAUSTRÁLIA
^cEUS$ 45 + US$ 5 de frete  (US$ 50 no total)

BudgetWare
P.O. Box 496
Newtown, NSW  2042
Australia

Telefone:  (02) 519-4233

Aceita:  MasterCard, Visa, Bankcard, cheques.
^P
^cBCANADÁ
^cE42 dólares canadenses (com frete etc.)

Distant Markets
Box 1149, 194 - 3803 Calgary Trail S.
Edmondton, Alb.  T6J 5M8
Canada

Pedidos:  1-800-661-7383

Aceita:  Visa, MasterCard, vale postal.
^P
^cBEUROPA (REINO UNIDO)
^cE30 libras + 5 de frete  (35 libras no total)

Precision Software Applications
Unit 3, Valley Court Offices
Lower Road,
Croydon, Near Royston
Harts.
SG8 0EF
United Kingdom

Telefone: +44 (0) 233 208 288
Fax: +44 (0) 233 208 089
Aceita:  Visa, MasterCard, Access etc.
^P
^cBALEMANHA
^cE49,-- DM mais 10,-- DM de frete.
(59,-- DM no total)

CDV-Software
Ettlingerstr. 5
7500 Karlsruhe 1
Germany

Telefone:  0721-22295

Aceita:  Visa, MasterCard, EuroCard, AmEx etc.
^P
^G100,10,16
Estes revendedores autorizados atendem você com educação e rapidez.  Pergunte também por outros ótimos programas da Apogee.

Se o seu país não está na lista, ligue:
 EUA (214) 278-5655.



É isso.  Divirta-se jogando Commander Keen!

^E)";

// About Id Software (chunk 4742)
constexpr char kHelpAboutId[] = R"(^P
^G20,224,79
^cBSobre a Id Software
^cE





O objetivo da Id Software é criar jogos de ponta, em tecnologia e em design.  A trilogia original do Keen foi o começo da nossa busca por jogos maiores e melhores no PC.  Não somos uma empresa que se acomoda.  O passado foi bom.  O presente é ótimo.  O futuro será incrível.  Essa é sempre a nossa visão.
^P
^G20,224,79
^cBA Id Software é:
^cE
^cFENGENHEIROS DE SOFTWARE
^cEJohn Carmack
John Romero
Jason Blochowiak

^cFARTE
^cEAdrian Carmack

^cFDIRETOR CRIATIVO
^cETom Hall

^cFPRESIDENTE
^cEMark Rein

^cFTrilha sonora de Robert Prince
^E)";

// The ending (chunk 4743)
constexpr char kEnding[] = R"(^P
^G8,30,49
^T48,108,62,100
^L100,8
Uau, que jeito impressionante de ativar este monolito da sabedoria antiga.
^T24,168,56,10
^T24,168,55,140
^T48,108,61,1
^T41,154,58,10
^T41,154,57,140
^T56,170,59,10
^T56,170,60,140
^P
^G30,80,50
^T70,120,51,150
^L100,30
 clique
^P
^G8,110,52
^L100,8
Silêncio, todos. O Oráculo vai falar.
^T8,110,53,280
^B100,8,300,20,8
^T8,110,54,110
Hmmm. Talvez esteja sem pilha ou algo assim.
^P
^G8,30,49
^T8,194,64,35
^T8,194,65,35
^T8,194,64,35
^T8,194,65,100
^P
^G8,30,66
^G8,60,72
^L100,8
Os Shikadi são uma raça de seres de sombra do outro lado da galáxia.
^P
^G8,30,66
^G8,60,71
^L100,8
Eles estão construindo uma Máquina do Armagedom para explodir a galáxia e depois refazê-la do jeito deles.
^P
^G8,30,66
^G8,60,73
^L100,8
Eles vivem no terceiro planeta do sistema Korath, a 71.000 anos-luz de Gnosticus IV. A Máquina do Armagedom está quase pronta.
^P
^G8,130,67
^L100,8
E esta é a cara deles.
^T8,130,68,100
^B100,8,300,20,8
^T8,130,69,70
^T8,130,68,70
^T8,130,69,70
^P
^G8,8,49
^T48,90,63,100
Esses Shikadi já eram!
^P
^G8,100,70
^L100,8
 Valeu pela dica, Oráculo.  Estou indo nessa.
^P
^G48,48,74
^L100,8
Tomara que não chova nos meus pais....
^P
^L100,8
Enquanto isso, na Terra...
^P
^G68,88,75
^P
^G88,88,76
^T88,88,77,140
^P
^G68,88,78
^P
Não perca "A Máquina do Armagedom", o final chocante de "Adeus, Galáxia!", onde você verá:

-- a cara dos Shikadi
-- o interior do "Omegamático"
-- muitos robôs e alienígenas esquisitos
-- cenas incríveis da galáxia explodindo
-- e mais uma ou duas grandes surpresas!

Ligue ou escreva já pedindo "A Máquina do Armagedom"!
^E)";

constexpr ChunkEntry kChunks[] = {
    {4739, kHelpComputerWrist},
    {4740, kHelpStory},
    {4741, kHelpGamePlay},
    {4742, kHelpAboutId},
    {4743, kEnding},
    {4745, kHelpOrder},
};

}  // namespace

extern const LanguageData kPortuguese = {
    "pt-BR",
    "PORTUGUÊS (BRASIL)",
    kExe,
    sizeof kExe / sizeof kExe[0],
    kText,
    sizeof kText / sizeof kText[0],
    kChunks,
    sizeof kChunks / sizeof kChunks[0],
};

}  // namespace keen4::lang
