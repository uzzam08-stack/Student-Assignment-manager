import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent

# Default location of compiled C shared library
DEFAULT_LIB_NAME = "libc_engine.dylib" if os.uname().sysname == "Darwin" else "libc_engine.so"
DEFAULT_C_LIB_PATH = os.environ.get(
    "SAPM_C_LIB_PATH",
    str(BASE_DIR / "c_engine" / "bin" / DEFAULT_LIB_NAME)
)

DEFAULT_DATA_FILEPATH = os.environ.get(
    "SAPM_DATA_FILEPATH",
    str(BASE_DIR / "assignments_data.csv")
)


class Config:
    """Base application configuration."""
    DEBUG = False
    TESTING = False
    C_LIB_PATH = DEFAULT_C_LIB_PATH
    DATA_FILEPATH = DEFAULT_DATA_FILEPATH
    PERSISTENCE_ENABLED = os.environ.get("SAPM_PERSISTENCE_ENABLED", "true").lower() in ("true", "1", "yes")
    JSON_SORT_KEYS = False


class DevelopmentConfig(Config):
    """Development configuration."""
    DEBUG = True


class TestingConfig(Config):
    """Test configuration using isolated ephemeral storage."""
    TESTING = True
    DEBUG = True
    DATA_FILEPATH = "/tmp/test_sapm_backend_data.csv"
    PERSISTENCE_ENABLED = True


class ProductionConfig(Config):
    """Production configuration."""
    DEBUG = False
    TESTING = False


CONFIG_MAP = {
    "development": DevelopmentConfig,
    "testing": TestingConfig,
    "production": ProductionConfig,
    "default": DevelopmentConfig,
}


def get_config(env_name: str | None = None) -> type[Config]:
    """Retrieves configuration class by name or from environment."""
    if env_name is None:
        env_name = os.environ.get("FLASK_ENV", "development").lower()
    return CONFIG_MAP.get(env_name, DevelopmentConfig)
