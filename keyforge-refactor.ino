#include <Adafruit_RGBLCDShield.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// NOW : use enums to track static list options:

void setup() {
  lcd.begin(16, 2);
  lcd.cursor();
  Serial.begin(9600);
}

enum GamePhases {
  titlePage,
  player1Prompt,
  player2Prompt,
  mainPlayPhase,
  chainsPrompt,
  drawPrompt,
  forgePrompt
};

class GameRules {
  public:

  const int baseKeyCost = 6;
  const int baseHandSize = 6;
  const int keyRange[2] = {0, 3};
  const int chainRange[2] = {0, 24};
  const int aemberRange[2] = {0, 99};
  const int forgeModRange[2] = {-99, 99};
  const int handRange[2] = {0, 36};

  int calcPenalty(int chains) {
    if (0 < chains && chains < 7) return -1;
    if (6 < chains && chains < 13) return -2;
    if (12 < chains && chains < 19) return -3;
    if (18 < chains) return -4;
    return 0;
  }
};

class CommonLogic {
  public:

  int calcChange(int currentValue, int delta, int minimum, int maximum) {
    int total = currentValue + delta;
    if (total < minimum) return minimum;
    if (total > maximum) return maximum;
    return total;
  }
};

const GameRules rules;
const CommonLogic commons;

class PlayerState {
  public: 

  String name;
  int chains = rules.chainRange[0];
  int aember = rules.aemberRange[0];
  int keys = rules.keyRange[0];

  PlayerState(String initName) {
    name = initName;
  }

  void changeAember(int delta) {
    aember = commons.calcChange(aember, delta, rules.aemberRange[0], rules.aemberRange[1]);
  }

  void changeKeys(int delta) {
    keys = commons.calcChange(keys, delta, rules.keyRange[0], rules.keyRange[1]);
  }

  void changeChains(int delta) {
    chains = commons.calcChange(chains, delta, rules.chainRange[0], rules.chainRange[1]);
  }

  void changeStat(int stat, int delta) {
    if (stat == 0) changeChains(delta);
    if (stat == 1) changeAember(delta);
    if (stat == 2) changeKeys(delta);
  }
};


class GameState {
  public:

  PlayerState player1 = PlayerState("P1");
  PlayerState player2 = PlayerState("P2");

  int currentStat = 0;
  int currentPhase = mainPlayPhase;

  int currentPlayer = 1;
  int currentPlayerForgeMod = 0;
  int currentPlayerHand = 0;
  int currentPlayerPenalty = 0;

  int calcKeyCost() {
    int total = rules.baseKeyCost + currentPlayerForgeMod;
    if (total < 0) return 0;
    return total;
  }

  void changeForgeMod(int delta) {
    currentPlayerForgeMod = commons.calcChange(currentPlayerForgeMod, delta, rules.forgeModRange[0], rules.forgeModRange[1]);
  }

  void changeHandSize(int delta) {
    currentPlayerHand = commons.calcChange(currentPlayerHand, delta, rules.handRange[0], rules.handRange[1]);
  }

  void setNextTurnState() {
    currentStat = 0;
    currentPlayer = currentPlayer == 1 ? 2 : 1;
    currentPlayerHand = 0;
    currentPlayerPenalty = 0;
  }

  void attemptForge() {
    if (currentPlayer == 1 && player1.aember >= calcKeyCost()) {
      player1.changeStat(2, 1);
      player1.changeStat(1, -calcKeyCost());
    }
    if (currentPlayer == 2 && player2.aember >= calcKeyCost()){
      player2.changeStat(2, 1);
      player2.changeStat(1, -calcKeyCost());
    }
  }

  void nextPhase() {
    bool p1SkipChains = currentPlayer == 1 && player1.chains == 0;
    bool p2SkipChains = currentPlayer == 2 && player2.chains == 0;

    switch(currentPhase) {
      // case titlePage:
      //   currentPhase = player1Prompt;
      //   break;
      // case player1Prompt:
      //   currentPhase = player2Prompt;
      //   break;
      // case player2Prompt:
      //   currentPhase = mainPlayPhase;
      //   break;

      case mainPlayPhase:
        if (p1SkipChains || p2SkipChains) {
          setNextTurnState();
          currentPhase = forgePrompt;
        } 
        else currentPhase = chainsPrompt;
        break;

      case chainsPrompt:
        currentPlayerPenalty = currentPlayer == 1 
          ? rules.calcPenalty(player1.chains) 
          : rules.calcPenalty(player2.chains);
        if (currentPlayerHand < rules.baseHandSize + currentPlayerPenalty) {
          if (currentPlayer == 1) player1.changeStat(0, -1);
          else player2.changeStat(0, -1);
        }
        currentPhase = drawPrompt;
        break;

      case drawPrompt:
        setNextTurnState();
        currentPhase = forgePrompt;
        break;

      case forgePrompt:
        attemptForge();
        currentPlayerForgeMod = 0;
        currentPhase = mainPlayPhase;
        break;
    }
  }

  void changePlayerStat(int delta) {
    if (currentPlayer == 1) player1.changeStat(currentStat, delta);
    else player2.changeStat(currentStat, delta);
  }
};

GameState game;

class GameVisuals {
  private:

  int p1offset = 0;
  int p2offset = 11;

  void renderLabels() {
    lcd.setCursor(6, 0);
    lcd.print("ch:p");
    lcd.setCursor(6, 1);
    lcd.print("ae:k");
  }

  void renderStatusCursor() {
    int offset = game.currentPlayer == 1 ? p1offset : p2offset;
    if (game.currentStat == 0) lcd.setCursor(1 + offset, 0);
    if (game.currentStat == 1) lcd.setCursor(1 + offset, 1);
    if (game.currentStat == 2) lcd.setCursor(3 + offset, 1);
  }
  
  void renderPlayerData(PlayerState thePlayer, int offset) {
    // print chains:penalty
    lcd.setCursor(offset, 0);
    if (thePlayer.chains < 10) lcd.print(" ");
    lcd.print(thePlayer.chains);
    lcd.print(":");
    lcd.print(rules.calcPenalty(thePlayer.chains));
    if (thePlayer.chains == 0) lcd.print(" ");
    // print aember:keys
    lcd.setCursor(offset, 1);
    if (thePlayer.aember < 10) lcd.print(" ");
    lcd.print(thePlayer.aember);
    lcd.print(":");
    lcd.print(thePlayer.keys);
  }

  int getDrawAmount(GameState game) {
    int drawCapacity = 
      rules.baseHandSize 
      + game.currentPlayerPenalty 
      - game.currentPlayerHand;

    return drawCapacity > 0 ? drawCapacity : 0;
  }

  String getPlayerName(GameState game) {
    return game.currentPlayer == 1 
      ? game.player1.name 
      : game.player2.name;
  }

  void hideCursor() {
    lcd.setCursor(16, 0);
  }

  public:

  void renderTitle() {
    lcd.print("KEYFORGE !");
  }

  void renderP1Prompt(GameState game) {
    lcd.print("p1 name");
  }

  void renderP2Prompt(GameState game) {
    lcd.print("p2 name");
  }

  void renderPlayers(GameState game) {
    renderLabels();
    renderPlayerData(game.player1, p1offset);
    renderPlayerData(game.player2, p2offset);
    renderStatusCursor();
  }

  void renderChainsPrompt(GameState game) {
    lcd.print("# of cards " + getPlayerName(game) + ":");
    if (game.currentPlayerHand < 10) lcd.print(" ");
    lcd.print(game.currentPlayerHand);
    lcd.setCursor(15,0);
  }

  void renderDrawCards(GameState game) {
    lcd.home();
    lcd.print(getPlayerName(game) + " draws ");
    lcd.print(getDrawAmount(game));
    hideCursor();
  }

  void renderForgePrompt(GameState game) {
    lcd.home();
    const int mod = game.currentPlayerForgeMod;
    lcd.print(getPlayerName(game) + " forge mod:");
    if (mod == 0) lcd.print("  ");
    if (mod > 0 && mod < 10) lcd.print(" +");
    if (mod > 9) lcd.print("+");
    if (mod < 0 && mod > -10) lcd.print(" ");
    lcd.print(mod);
    lcd.setCursor(15, 0);
  }

  bool isGameOver(GameState game) {
    if (game.player1.keys == 3) return true;
    if (game.player2.keys == 3) return true;
    return false;
  }

  void renderGameOver(GameState game) {
    String winner;
    if (game.player1.keys == 3) winner = game.player1.name;
    if (game.player2.keys == 3) winner = game.player2.name;
    lcd.print(winner);
    lcd.print(" wins!");
  }

  void render(GameState game) {
    lcd.home();
    if (isGameOver(game)) renderGameOver(game);
    else {
      if (game.currentPhase == titlePage) renderTitle();
      if (game.currentPhase == player1Prompt) renderP1Prompt(game);
      if (game.currentPhase == player2Prompt) renderP2Prompt(game);
      if (game.currentPhase == mainPlayPhase) renderPlayers(game);
      if (game.currentPhase == chainsPrompt) renderChainsPrompt(game);
      if (game.currentPhase == drawPrompt) renderDrawCards(game);
      if (game.currentPhase == forgePrompt) renderForgePrompt(game);
    }
  }
};

GameVisuals visuals;

int lastButton;
unsigned long lastRender = 0;

void loop() {
  const int currentButtons = lcd.readButtons();

  if (currentButtons != lastButton) {
    if (currentButtons == BUTTON_SELECT) {
      lcd.clear();
      game.nextPhase();
    }
    if (currentButtons == BUTTON_LEFT) {
      game.currentStat--;
      if (game.currentStat < 0) {
        game.currentStat = 2;
      }
    }
    if (currentButtons == BUTTON_RIGHT) {
      game.currentStat++;
      game.currentStat %= 3;
    }
    if (currentButtons == BUTTON_UP) {
      if (game.currentPhase == mainPlayPhase) game.changePlayerStat(1);
      if (game.currentPhase == chainsPrompt) game.changeHandSize(1);
      if (game.currentPhase == forgePrompt) game.changeForgeMod(1);
    }
    if (currentButtons == BUTTON_DOWN) {
      if (game.currentPhase == mainPlayPhase) game.changePlayerStat(-1);
      if (game.currentPhase == chainsPrompt) game.changeHandSize(-1);
      if (game.currentPhase == forgePrompt) game.changeForgeMod(-1);
    }
    // toggle between players:
    if (currentButtons == BUTTON_RIGHT + BUTTON_LEFT) {
      game.currentPlayer = game.currentPlayer == 1 ? 2 : 1;
    }
  }

  lastButton = currentButtons;
  unsigned long currentTime = millis();

  if (currentTime - lastRender > 300) {
    lastRender = currentTime;
    visuals.render(game);
  }
}
