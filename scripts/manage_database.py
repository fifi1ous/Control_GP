import os
import sqlite3

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


class ManageDatabase:
    # Anchor the DB path to this script's location, not the process CWD,
    # so it resolves correctly regardless of where the launcher runs from.
    NAME_OF_DB = os.path.normpath(
        os.path.join(_SCRIPT_DIR, "..", "database", "CadastralInfoDB.db")
    )

    # ---- Cadastral unit table ----
    TABLE_CADASTRAL_UNIT = "cadastral_unit"
    COLUMN_CADASTRAL_UNIT_ID = "cadastral_unit_id"
    COLUMN_CADASTRAL_UNIT_NAME = "cadastral_unit_name"

    # ---- Surveyor table ----
    TABLE_SURVEYORS = "surveyors"
    COLUMN_AZI_NUMBER = "AziNumber"
    COLUMN_FIRST_NAME = "FirstName"
    COLUMN_LAST_NAME = "LastName"
    COLUMN_AUTHORIZATION = "Authorization"
    COLUMN_IS_ACTIVE = "IsActive"


    @classmethod
    def set_db_path(cls, path: str) -> None:
        """
        Override the default database path/filename.

        Example:
            ManageDatabase.set_db_path("/data/myapp/CadastralInfoDB.db")
        """
        cls.NAME_OF_DB = path


    @staticmethod
    def get_cadastral_unit_name_by_id(cadastral_unit_id):
        with sqlite3.connect(ManageDatabase.NAME_OF_DB) as conn:
            cursor = conn.cursor()
            cursor.execute(
                f"SELECT {ManageDatabase.COLUMN_CADASTRAL_UNIT_NAME} "
                f"FROM {ManageDatabase.TABLE_CADASTRAL_UNIT} "
                f"WHERE {ManageDatabase.COLUMN_CADASTRAL_UNIT_ID} = ?",
                (cadastral_unit_id,)
            )
            result = cursor.fetchone()

        return result[0] if result else None

    @staticmethod
    def get_cadastral_unit_id_by_name(cadastral_unit_name):
        with sqlite3.connect(ManageDatabase.NAME_OF_DB) as conn:
            cursor = conn.cursor()
            cursor.execute(
                f"SELECT {ManageDatabase.COLUMN_CADASTRAL_UNIT_ID} "
                f"FROM {ManageDatabase.TABLE_CADASTRAL_UNIT} "
                f"WHERE {ManageDatabase.COLUMN_CADASTRAL_UNIT_NAME} = ?",
                (cadastral_unit_name,)
            )
            result = cursor.fetchone()

        return result[0] if result else None


    @staticmethod
    def get_surveyor_by_azi_number(azinumber: int) -> dict | None:
        """
        Returns a dict with FirstName, LastName, Authorization for the given AziNumber,
        or None if not found.
        """
        with sqlite3.connect(ManageDatabase.NAME_OF_DB) as conn:
            cursor = conn.cursor()
            cursor.execute(
                f"""
                SELECT
                    {ManageDatabase.COLUMN_FIRST_NAME},
                    {ManageDatabase.COLUMN_LAST_NAME},
                    {ManageDatabase.COLUMN_AUTHORIZATION}
                FROM {ManageDatabase.TABLE_SURVEYORS}
                WHERE {ManageDatabase.COLUMN_AZI_NUMBER} = ?
                """,
                (azinumber,)
            )
            row = cursor.fetchone()

        if row:
            return {
                "FirstName": row[0],
                "LastName": row[1],
                "Authorization": row[2],
            }
        return None

    @staticmethod
    def get_surveyor_by_name(first_name: str, last_name: str) -> dict | None:
        """
        Returns a dict with AziNumber, Authorization for the given name,
        or None if not found.
        """
        with sqlite3.connect(ManageDatabase.NAME_OF_DB) as conn:
            cursor = conn.cursor()
            cursor.execute(
                f"""
                SELECT
                    {ManageDatabase.COLUMN_AZI_NUMBER},
                    {ManageDatabase.COLUMN_AUTHORIZATION}
                FROM {ManageDatabase.TABLE_SURVEYORS}
                WHERE {ManageDatabase.COLUMN_FIRST_NAME} = ?
                AND {ManageDatabase.COLUMN_LAST_NAME} = ?
                """,
                (first_name, last_name)
            )
            row = cursor.fetchone()

        if row:
            return {
                "AziNumber": row[0],
                "Authorization": row[1],
            }
        return None

